# Lab F: Profile and Optimize a ReAct Agent

> **Goal:** Take a baseline ~150-LoC ReAct agent, instrument it with OpenTelemetry-style spans, and apply three classical OS levers — **parallelism, caching, and timeout + LLM-mediated fallback** — to improve end-to-end latency. Produce before/after measurements (p50 / p99) for each lever and a one-paragraph diagnosis per part.

---

## Prerequisites

### What you should know

* The ReAct loop from §1 (Thought → Action → Observation → repeat).

* The latency identity: $T_{\text{agent}} = \sum T_{\text{LLM}} + \sum T_{\text{tool}} + T_{\text{orch}}$.

* The three levers from §4 and *which term in the identity each one attacks*.

* Python `asyncio` basics (`async def`, `await`, `asyncio.gather`).

### What you need

* Python 3.10+

* Standard library only (no API keys, no `pip install` required for the basic tier)

* Optional: `matplotlib` if you want to render waterfall plots as PNGs (text waterfalls are also fine)

### Verify your environment

```bash
python3 --version          # 3.10+
python3 -c "import asyncio, json, time, hashlib; print('OK')"
```

### Directory layout

```bash
mkdir -p ~/lab_f && cd ~/lab_f
mkdir -p data traces reports
```

Create a small text corpus the agent will analyze:

```bash
cat > data/doc1.txt <<'EOF'
Operating systems abstract hardware. Page caches, schedulers,
and file systems each balance throughput against tail latency.
EOF

cat > data/doc2.txt <<'EOF'
Concurrency primitives include locks, condition variables,
and semaphores. Choosing the wrong primitive often hurts p99.
EOF

cat > data/doc3.txt <<'EOF'
Tail latency in storage is dominated by fsync. The page cache
hides most reads, but durable writes must hit disk.
EOF
```

---

## Part 0: Starter Code (5 min)

Save the following as `agent.py`. This is your baseline — **do not modify it for Part 1**; you only add instrumentation around it. Later parts will edit copies of it.

```python
"""
agent.py — minimal sequential ReAct agent (~150 LoC).
Mock LLM; local tools; no network.
"""

import json, time, random
from typing import Any, Callable, Optional

# --------------- Mock LLM ---------------
# A scripted "LLM" that returns a fixed sequence of tool_calls / final_answer.
# Each chat() call simulates: prefill (scales with input bytes) + decode (fixed).

class MockLLM:
    def __init__(self, script: list[dict],
                 decode_s: float = 0.7,
                 prefill_us_per_char: float = 10.0):
        self.script = script
        self.step = 0
        self.decode_s = decode_s
        self.prefill_us_per_char = prefill_us_per_char

    def chat(self, messages: list[dict]) -> dict:
        input_chars = sum(len(json.dumps(m, default=str)) for m in messages)
        time.sleep(input_chars * self.prefill_us_per_char / 1e6)  # prefill
        time.sleep(self.decode_s)                                  # decode
        if self.step >= len(self.script):
            return {"role": "assistant", "tool_calls": None,
                    "content": "Done.", "input_tokens": input_chars // 4,
                    "output_tokens": 1}
        out = dict(self.script[self.step])
        out["input_tokens"] = input_chars // 4
        out["output_tokens"] = 50
        self.step += 1
        return out


# --------------- Tools ---------------
# Pure (no side effects) unless noted.

def tool_read_file(path: str) -> dict:
    time.sleep(0.005)                                              # ~5 ms
    with open(path) as f:
        return {"path": path, "content": f.read()}

def tool_word_count(text: str) -> dict:
    time.sleep(0.01)                                               # ~10 ms
    return {"word_count": len(text.split())}

def tool_classify(text: str) -> dict:
    time.sleep(0.05)                                               # ~50 ms
    label = "tail_latency" if "tail" in text.lower() else "general"
    return {"label": label, "len": len(text)}

def tool_summarize(text: str) -> dict:
    time.sleep(0.20)                                               # ~200 ms
    words = text.split()
    return {"summary": " ".join(words[:25]) +
            ("..." if len(words) > 25 else "")}

TOOLS: dict[str, Callable[..., dict]] = {
    "read_file": tool_read_file,
    "word_count": tool_word_count,
    "classify": tool_classify,
    "summarize": tool_summarize,
}


# --------------- Sequential ReAct loop ---------------

def run_agent(llm: MockLLM, user_goal: str,
              tools: dict = TOOLS, max_steps: int = 12) -> dict:
    """
    Runs the ReAct loop. Returns {answer, messages}.
    Each step: LLM produces tool_calls (or final). Runtime executes them.
    """
    messages = [{"role": "user", "content": user_goal}]
    answer = None
    for _ in range(max_steps):
        resp = llm.chat(messages)
        messages.append({"role": "assistant",
                         "tool_calls": resp.get("tool_calls"),
                         "content": resp.get("content")})
        calls = resp.get("tool_calls")
        if not calls:
            answer = resp.get("content")
            break
        for c in calls:
            name, args = c["name"], c["arguments"]
            if name == "final_answer":
                answer = args.get("text", "")
                return {"answer": answer, "messages": messages}
            fn = tools[name]
            result = fn(**args)
            messages.append({"role": "tool", "name": name,
                             "content": result})
    return {"answer": answer, "messages": messages}


# --------------- Default scripted task ---------------
# Goal: "Classify and summarize each of the three docs in ./data"

DEFAULT_SCRIPT = [
    # Turn 1: read doc1
    {"tool_calls": [{"name": "read_file",
                     "arguments": {"path": "data/doc1.txt"}}]},
    # Turn 2: classify doc1
    {"tool_calls": [{"name": "classify",
                     "arguments": {"text": "<<doc1>>"}}]},
    # Turn 3: read doc2
    {"tool_calls": [{"name": "read_file",
                     "arguments": {"path": "data/doc2.txt"}}]},
    # Turn 4: classify doc2
    {"tool_calls": [{"name": "classify",
                     "arguments": {"text": "<<doc2>>"}}]},
    # Turn 5: read doc3
    {"tool_calls": [{"name": "read_file",
                     "arguments": {"path": "data/doc3.txt"}}]},
    # Turn 6: classify doc3
    {"tool_calls": [{"name": "classify",
                     "arguments": {"text": "<<doc3>>"}}]},
    # Turn 7: summarize all three concatenated
    {"tool_calls": [{"name": "summarize",
                     "arguments": {"text": "<<all>>"}}]},
    # Turn 8: final answer
    {"tool_calls": [{"name": "final_answer",
                     "arguments": {"text": "Three docs analyzed."}}]},
]

if __name__ == "__main__":
    llm = MockLLM(DEFAULT_SCRIPT)
    t0 = time.perf_counter()
    out = run_agent(llm, "Classify and summarize the three docs in ./data")
    elapsed = time.perf_counter() - t0
    print(f"answer  : {out['answer']}")
    print(f"messages: {len(out['messages'])}")
    print(f"wall    : {elapsed:.2f} s")
```

### Sanity check

```bash
python3 agent.py
```

Expected output (your numbers will be ±0.3 s):

```
answer  : Three docs analyzed.
messages: 16
wall    : 6.1 s
```

> **Stop and predict:** Before instrumenting, write down on paper your prediction of where the time went. *How many LLM calls? How many tool calls? Which dominates?* You'll grade your intuition against the trace in Part 1.

---

## Part 1 — Profile: OTel-Style Spans and a Waterfall (25 min)

### What you'll do

Add instrumentation to `agent.py` so every LLM call and every tool call emits a JSON span with the OpenTelemetry GenAI conventions from the lecture. Stream spans to a JSONL trace file. Render a waterfall and write a one-paragraph diagnosis.

> **Why are we hand-writing a tracer?** In production, you wouldn't. Real teams `pip install` an auto-instrumentation library — `openllmetry`, `openinference`, the official OpenAI Agents SDK, or LangChain's built-in tracing — and get OTel `gen_ai.*` spans for free, then ship them to a dashboard like LangSmith, Langfuse, or Arize Phoenix. We are writing the tracer ourselves so you can see exactly what a span is and how parent–child causality is recorded. The ~30 lines you're about to write are the conceptual core that those libraries hide behind one `Instrumentor().instrument()` call. If you understand this, you can debug what the dashboards show you; if you skip it, you're at the mercy of the UI.

### 1.1 Add a tiny span recorder

Create `tracer.py`:

```python
"""
tracer.py — minimal OTel-shaped span recorder.
Writes one JSON object per span to a JSONL file.
"""
import json, time, uuid, threading, contextvars
from contextlib import contextmanager
from typing import Optional

_lock = threading.Lock()
_current_parent: contextvars.ContextVar[Optional[str]] = (
    contextvars.ContextVar("current_parent", default=None))

class Tracer:
    def __init__(self, path: str):
        self.path = path
        # Truncate at start of each run.
        open(self.path, "w").close()

    @contextmanager
    def span(self, name: str, **attrs):
        span_id = uuid.uuid4().hex[:8]
        parent = _current_parent.get()
        token = _current_parent.set(span_id)
        t0 = time.perf_counter()
        record = {"span_id": span_id, "parent": parent, "name": name,
                  "start_s": t0, "attrs": dict(attrs)}
        try:
            yield record
        finally:
            record["end_s"] = time.perf_counter()
            record["duration_ms"] = (record["end_s"] - t0) * 1000
            with _lock, open(self.path, "a") as f:
                f.write(json.dumps(record, default=str) + "\n")
            _current_parent.reset(token)
```

> **Why so small?** A span recorder is just *(name, start, end, parent, attrs) → JSON line*. Frameworks add a lot of features on top, but the core is what you just wrote.
>
> **Why** **`contextvars`** **and not a** **`self._stack`** **list?** In Part 1 either works — the agent is single-threaded synchronous. But in Parts 2-4 you'll launch tool calls concurrently with `asyncio.gather`. A shared list races: if coro A enters its span and then coro B enters, B sees A on top of the stack and incorrectly records A as its parent. `contextvars.ContextVar` is the standard fix: each `asyncio.Task` copies the current context at creation, so each concurrent tool span correctly attributes parentage to the enclosing `agent.task` span. The real OpenTelemetry SDK uses the same pattern.

### 1.2 Instrument the agent

Make a copy: `cp agent.py agent_p1.py` and edit it. At the top, import the tracer and create one module-level instance:

```python
from tracer import Tracer
tracer = Tracer("traces/baseline.jsonl")
```

(For Parts 2-4 you'll change the path to `traces/parallel.jsonl`, `traces/flaky_with_timeout.jsonl`, `traces/cached.jsonl` respectively.)

Then wrap each LLM call and each tool call in a span. Required attributes (use these exact keys — they match the OTel GenAI conventions shown in the lecture):

| Span name               | Required attributes                                                                                                                                         |
| ----------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `agent.task`            | `user_goal`                                                                                                                                                 |
| `gen_ai.client.request` | `gen_ai.system="mock"`, `gen_ai.request.model="mock-1"`, `gen_ai.usage.input_tokens`, `gen_ai.usage.output_tokens`, `gen_ai.response.finish_reasons` (list) |
| `tool.call`             | `tool.name`, `tool.args_hash` (8-char SHA256 prefix of canonical JSON args), `tool.status` (`ok` / `error`)                                                 |

The two changes you need in `run_agent`:

```python
import hashlib
def args_hash(args: dict) -> str:
    return hashlib.sha256(
        json.dumps(args, sort_keys=True, default=str).encode()
    ).hexdigest()[:8]

# Around llm.chat(...):
with tracer.span("gen_ai.client.request",
                 **{"gen_ai.system": "mock",
                    "gen_ai.request.model": "mock-1"}) as s:
    resp = llm.chat(messages)
    s["attrs"]["gen_ai.usage.input_tokens"]  = resp["input_tokens"]
    s["attrs"]["gen_ai.usage.output_tokens"] = resp["output_tokens"]
    s["attrs"]["gen_ai.response.finish_reasons"] = (
        ["tool_calls"] if resp.get("tool_calls") else ["stop"])

# Around fn(**args):
with tracer.span("tool.call",
                 **{"tool.name": name,
                    "tool.args_hash": args_hash(args)}) as s:
    try:
        result = fn(**args)
        s["attrs"]["tool.status"] = "ok"
    except Exception as e:
        s["attrs"]["tool.status"] = "error"
        s["attrs"]["error"] = str(e)
        raise

# Around the whole loop body:
with tracer.span("agent.task", user_goal=user_goal):
    ...  # existing loop
```

Run it:

```bash
python3 agent_p1.py        # writes traces/baseline.jsonl
```

### 1.3 Render the waterfall

Create `waterfall.py`:

```python
"""
waterfall.py — render a JSONL trace as a text waterfall.
Each span is one row; X axis is time in ms; bar = duration.
"""
import json, sys

def render(path: str, width: int = 60) -> None:
    spans = [json.loads(l) for l in open(path)]
    if not spans:
        print("(empty trace)"); return
    t0 = min(s["start_s"] for s in spans)
    total_ms = max(s["end_s"] for s in spans) - t0
    total_ms *= 1000
    print(f"trace: {path}   total: {total_ms:.0f} ms   spans: {len(spans)}")
    print("-" * (width + 30))
    spans.sort(key=lambda s: s["start_s"])
    for s in spans:
        start_ms = (s["start_s"] - t0) * 1000
        dur_ms   = s["duration_ms"]
        left  = int(start_ms / total_ms * width)
        barlen = max(1, int(dur_ms / total_ms * width))
        bar = " " * left + "█" * barlen
        label = s["name"]
        if s["name"] == "tool.call":
            label = f"tool:{s['attrs'].get('tool.name','?')}"
        print(f"{label:<24}{bar:<{width}} {dur_ms:7.1f} ms")

if __name__ == "__main__":
    render(sys.argv[1] if len(sys.argv) > 1 else "traces/baseline.jsonl")
```

```bash
python3 waterfall.py traces/baseline.jsonl
```

You should see roughly:

```
agent.task              ████████████████████████████████████████████  6612 ms
gen_ai.client.request   █                                              714 ms
tool:read_file          ▏                                                7 ms
gen_ai.client.request    █                                              722 ms
tool:classify           ▏                                               52 ms
...
tool:summarize             █                                            205 ms
```

### 1.4 Diagnose

Compute, from your trace:

| Term                                              | Value  |
| ------------------------------------------------- | ------ |
| Number of LLM calls                               | <br /> |
| Number of tool calls                              | <br /> |
| $\sum T_{\text{LLM}}$                             | <br /> |
| $\sum T_{\text{tool}}$                            | <br /> |
| $T_{\text{orchestration}}$ (= total − LLM − tool) | <br /> |
| Dominant term (%)                                 | <br /> |

> **Deliverable for Part 1 (write into** **`reports/perf_report.md`):**
>
> 1. The per-term table above.
> 2. One sentence naming which of the four lecture patterns this trace matches (prefill explosion / slow-tool tail / sequential dependency / repeated work) and why.
> 3. One sentence predicting which of the three levers (parallelism / cache / timeout) you expect to help most, and why.

---

## Part 2 — Parallelize: Same-Turn Parallel Tool Calls (20 min)

### The setup

The baseline alternates `read_file` → `classify` → `read_file` → `classify` → ... — six sequential tool calls plus the LLM round-trips between them. But the three reads are independent of each other, and the three classifies are independent. They form a **wide DAG** (slide §4a): we can fold them into two LLM turns instead of six.

### 2.1 Make a parallel copy

```bash
cp agent_p1.py agent_p2.py
```

Three changes:

**(a) Async tool dispatch.** Wrap your tool functions so that within one turn, all `tool_calls` are launched concurrently:

```python
import asyncio

async def _run_one_tool(name: str, args: dict, tools: dict, tracer: Tracer):
    with tracer.span("tool.call",
                     **{"tool.name": name,
                        "tool.args_hash": args_hash(args)}) as s:
        # Run sync tool in a thread so blocking sleep doesn't stall the loop.
        result = await asyncio.to_thread(tools[name], **args)
        s["attrs"]["tool.status"] = "ok"
        return name, result

async def dispatch_calls(calls: list[dict], tools: dict, tracer: Tracer):
    coros = [_run_one_tool(c["name"], c["arguments"], tools, tracer)
             for c in calls if c["name"] != "final_answer"]
    return await asyncio.gather(*coros)
```

**(b) New script that batches.** Instead of 7 turns of 1 tool call each, use 3 turns:

```python
PARALLEL_SCRIPT = [
    # Turn 1: read all three docs in parallel.
    {"tool_calls": [
        {"name": "read_file", "arguments": {"path": "data/doc1.txt"}},
        {"name": "read_file", "arguments": {"path": "data/doc2.txt"}},
        {"name": "read_file", "arguments": {"path": "data/doc3.txt"}},
    ]},
    # Turn 2: classify all three in parallel.
    {"tool_calls": [
        {"name": "classify", "arguments": {"text": "<<doc1>>"}},
        {"name": "classify", "arguments": {"text": "<<doc2>>"}},
        {"name": "classify", "arguments": {"text": "<<doc3>>"}},
    ]},
    # Turn 3: summarize (serial — depends on classify results).
    {"tool_calls": [{"name": "summarize",
                     "arguments": {"text": "<<all>>"}}]},
    # Turn 4: final
    {"tool_calls": [{"name": "final_answer",
                     "arguments": {"text": "Three docs analyzed."}}]},
]
```

**(c) Make** **`run_agent`** **itself async, with a single event loop for the whole run.** This is the change Part 3's timeout will depend on. If you keep `run_agent` synchronous and call `asyncio.run(dispatch_calls(...))` once *per turn*, every per-turn event loop tries to shut down its default executor when it returns — and that shutdown blocks until any background tool threads finish. A 2 s slow tool that you "cancelled" with `wait_for(timeout=1.0)` will still hold the per-turn shutdown for a full 2 s, which silently defeats the timeout. With one long-lived event loop covering the whole run, the orphan thread finishes in the background while subsequent turns proceed.

Replace the body of `run_agent` with:

```python
async def run_agent(llm: MockLLM, user_goal: str, tracer: Tracer,
                    tools: dict = TOOLS, max_steps: int = 12) -> dict:
    messages = [{"role": "user", "content": user_goal}]
    with tracer.span("agent.task", user_goal=user_goal):
        for _ in range(max_steps):
            with tracer.span("gen_ai.client.request",
                             **{"gen_ai.system": "mock",
                                "gen_ai.request.model": "mock-1"}) as s:
                resp = llm.chat(messages)
                s["attrs"]["gen_ai.usage.input_tokens"]  = resp["input_tokens"]
                s["attrs"]["gen_ai.usage.output_tokens"] = resp["output_tokens"]
                s["attrs"]["gen_ai.response.finish_reasons"] = (
                    ["tool_calls"] if resp.get("tool_calls") else ["stop"])
            messages.append({"role": "assistant",
                             "tool_calls": resp.get("tool_calls"),
                             "content": resp.get("content")})
            calls = resp.get("tool_calls")
            if not calls:
                return {"answer": resp.get("content"), "messages": messages}
            # Detect final_answer BEFORE dispatching so we don't run it as a tool.
            for c in calls:
                if c["name"] == "final_answer":
                    return {"answer": c["arguments"].get("text", ""),
                            "messages": messages}
            results = await dispatch_calls(calls, tools, tracer)
            for name, result in results:
                messages.append({"role": "tool", "name": name,
                                 "content": result})
    return {"answer": None, "messages": messages}

if __name__ == "__main__":
    tracer = Tracer("traces/parallel.jsonl")
    llm = MockLLM(PARALLEL_SCRIPT)
    t0 = time.perf_counter()
    out = asyncio.run(run_agent(
        llm, "Classify and summarize the three docs in ./data", tracer))
    print(f"wall: {time.perf_counter() - t0:.2f} s")
```

Run and trace:

```bash
python3 agent_p2.py        # writes traces/parallel.jsonl
python3 waterfall.py traces/parallel.jsonl
```

You should see the three `read_file` bars **stacked at the same time offset**, and same for the three `classify` bars — the visual signature of parallel dispatch.

### 2.2 Measure

Run each variant 5 times and record wall time. Use `time.perf_counter` around `run_agent`.

| Variant           | Run 1  | Run 2  | Run 3  | Run 4  | Run 5  | Mean   |
| ----------------- | ------ | ------ | ------ | ------ | ------ | ------ |
| Baseline (Part 1) | <br /> | <br /> | <br /> | <br /> | <br /> | <br /> |
| Parallel (Part 2) | <br /> | <br /> | <br /> | <br /> | <br /> | <br /> |

Compute the speedup. Then compute the **Amdahl ceiling**: from your Part 1 numbers, what fraction $p$ of total time was spent in tools? What's $1 / (1 - p)\$ — the upper bound on speedup if tool time went to zero?

> **A wrinkle to address explicitly.** You will likely measure a speedup *larger* than the Amdahl ceiling. That's not a measurement error — PARALLEL\_SCRIPT also folded 6 LLM rounds into 2, so you simultaneously got (a) parallel tool execution AND (b) fewer LLM round-trips. Pure tool parallelism would have given you a tiny win (LLM dominates Part 1's total). The dominant effect here is round-folding, which is what "wide-DAG batching" actually delivers in real agent systems: you can't get parallel tool calls without telling the model "here are N things you can do at once," which by construction is one fewer LLM turn.

> **Deliverable for Part 2 (`reports/perf_report.md`):**
>
> 1. The 5×2 table of wall times + mean speedup.
> 2. Amdahl ceiling for tool-only parallelism, observed speedup, and a sentence attributing the gap to round-folding (or whatever else you find).
> 3. One paragraph: *"Why didn't parallelism move p99 much? What kind of task would it have moved?"* (Hint: see slide "Putting It Together".)

---

## Part 3 — Tame the Tail: Timeout + LLM-Mediated Fallback (25 min)

### The setup

Real tools have heavy tails. We'll inject one. Add this tool to your `agent_p2.py` (rename the file to `agent_p3.py` first):

```bash
cp agent_p2.py agent_p3.py
```

```python
def tool_classify_flaky(text: str) -> dict:
    """Same as classify, but 10% of the time takes 2.0 s instead of 50 ms."""
    if random.random() < 0.10:
        time.sleep(2.0)
    else:
        time.sleep(0.05)
    return {"label": "neutral", "len": len(text)}

TOOLS["classify_flaky"] = tool_classify_flaky
```

Update the script to use `classify_flaky` instead of `classify`. Set `random.seed()` to a different value each run so you actually sample the tail.

### 3.1 Measure the baseline tail

Run the agent **30 times** and record wall time per run. Compute p50, p95, p99. (For 30 samples, p99 = max; that's fine for the lab — note the limitation in your report.) `run_agent` is async, so each invocation goes through `asyncio.run`:

```python
import statistics

def run_once():
    llm = MockLLM(PARALLEL_SCRIPT_FLAKY)
    t0 = time.perf_counter()
    asyncio.run(run_agent(llm, "Classify and summarize ...", tracer))
    return time.perf_counter() - t0

times = sorted(run_once() for _ in range(30))
p50, p95, p99 = times[15], times[28], times[29]
```

Plot a histogram (matplotlib or just a text histogram) — you should see most runs near ~3.0 s with a few outliers near ~5.0 s. **This is the tail.**

### 3.2 Add timeout + LLM fallback

The lecture's pattern (slide *"Defense: Timeout + LLM-Driven Fallback"*):

1. Give every tool a deadline.
2. On timeout, **don't raise** — return a structured observation.
3. Let the LLM decide what to do next.

For this lab, since the LLM is scripted, *we* play the role of "what the LLM decides." Add a fallback rule: when a `tool.call` returns `{"error": "timeout"}`, the runtime substitutes a degraded result and continues.

Modify `_run_one_tool`:

```python
TIMEOUT_S = 1.0

async def _run_one_tool(name, args, tools, tracer):
    with tracer.span("tool.call",
                     **{"tool.name": name,
                        "tool.args_hash": args_hash(args)}) as s:
        try:
            result = await asyncio.wait_for(
                asyncio.to_thread(tools[name], **args),
                timeout=TIMEOUT_S)
            s["attrs"]["tool.status"] = "ok"
            return name, result
        except asyncio.TimeoutError:
            s["attrs"]["tool.status"] = "timeout"
            s["attrs"]["timeout_s"] = TIMEOUT_S
            # The "LLM fallback" — for the lab, a deterministic stand-in.
            return name, {"error": "timeout", "elapsed_s": TIMEOUT_S,
                          "fallback_label": "unknown",
                          "hint": "tool unavailable; downstream proceeded"}
```

> **Why the structured timeout matters:** if you raise the exception instead, the agent loop dies and the user sees nothing. Returning data lets the loop continue with degraded information — which is exactly what the slide's "the LLM is your fallback policy" insight argues for.

> **Orphan-thread caveat — read this even if your numbers look right.** When `asyncio.wait_for` raises `TimeoutError`, the underlying `asyncio.to_thread(...)` is cancelled only at the asyncio level: the OS thread keeps running the original `time.sleep(2.0)` in the executor pool until it returns naturally. We get the wall-time win because `run_agent` is async with a single event loop (§2.1c), so the orphan finishes in the background while the next turn proceeds. Two consequences worth remembering:
>
> 1. If you regressed `run_agent` to synchronous and called `asyncio.run` per turn, the per-turn loop's executor shutdown would block until the orphan finishes — silently undoing the timeout. This is a common production bug.
> 2. In real systems an orphan thread can hold a database connection, write half a file, or burn API quota. Production code combines `wait_for` with cooperative cancellation (a `cancel_token` the tool checks) and resource-cap circuit breakers. Out of scope for the lab, but mention it in your report's open-question paragraph.

### 3.3 Re-measure

Run 30 times again with the new timeout in place. Record p50 / p95 / p99.

| Variant              | p50 (s) | p95 (s) | p99 (s) | # timeouts observed |
| -------------------- | ------- | ------- | ------- | ------------------- |
| Flaky-tool baseline  | <br />  | <br />  | <br />  | <br />              |
| + timeout + fallback | <br />  | <br />  | <br />  | <br />              |

Open `traces/flaky_with_timeout.jsonl` from one of the runs that hit a timeout. Find the `tool.call` span with `tool.status = "timeout"`. Verify its duration ≈ `TIMEOUT_S × 1000` ms.

> **Deliverable for Part 3 (`reports/perf_report.md`):**
>
> 1. The p50 / p95 / p99 table above.
> 2. One paragraph: *"Did p50 change? Why or why not? Did p99 change? By how much?"*
> 3. One paragraph: *"What would happen to correctness — to the final answer's accuracy — if the flaky tool was actually load-bearing? How would you defend against silently-wrong fallbacks?"* (This is open-ended; the lecture only sketched the answer.)

---

## Part 4 — Cache: Tool-Result Cache for Repeated Work (15 min)

### The setup

Real agent workloads have heavy repeat structure: the same file is read multiple times across turns; the same query is classified again. The cache key from the slide is `tool_name + args_hash` — and you already compute `args_hash` in your tracer (Part 1).

### 4.1 Pick what to cache

From the lecture's "When NOT to Cache" slide:

| Tool                        | Cache it? | Why                                               |
| --------------------------- | --------- | ------------------------------------------------- |
| `read_file`                 | Yes       | Pure read                                         |
| `word_count`                | Yes       | Pure                                              |
| `classify`                  | Yes       | Pure                                              |
| `summarize`                 | Yes       | Pure                                              |
| `classify_flaky`            | **Maybe** | Pure but stochastic — caching freezes one outcome |
| (hypothetical) `write_file` | **No**    | Side effect                                       |

For the lab, cache the four pure tools.

### 4.2 Add an in-process cache

```bash
cp agent_p3.py agent_p4.py
```

> **First, revert the script back to** **`classify`** **(not** **`classify_flaky`).** Part 3 changed PARALLEL\_SCRIPT to use `classify_flaky` to demonstrate timeouts; Part 4 wants a clean cache demo, so we want a deterministic tool. Caching a stochastic tool *is* a real question — it's question 4.4(2) below — but you'll answer it in prose, not by demoing it. Edit PARALLEL\_SCRIPT in `agent_p4.py` so all three classify calls use `"classify"`.

```python
_TOOL_CACHE: dict[str, dict] = {}
CACHEABLE = {"read_file", "word_count", "classify", "summarize"}

async def _run_one_tool(name, args, tools, tracer):
    key = f"{name}:{args_hash(args)}"
    if name in CACHEABLE:
        if key in _TOOL_CACHE:
            with tracer.span("tool.call",
                             **{"tool.name": name,
                                "tool.args_hash": args_hash(args),
                                "tool.cache": "hit",
                                "tool.status": "ok"}):
                return name, _TOOL_CACHE[key]
        cache_attr = {"tool.cache": "miss"}
    else:
        # Tools we deliberately don't cache (e.g. classify_flaky if you keep it
        # around). Mark distinctly so cache-hit/miss counts stay meaningful.
        cache_attr = {"tool.cache": "skip"}
    with tracer.span("tool.call",
                     **{"tool.name": name,
                        "tool.args_hash": args_hash(args),
                        **cache_attr}) as s:
        try:
            result = await asyncio.wait_for(
                asyncio.to_thread(tools[name], **args),
                timeout=TIMEOUT_S)
            s["attrs"]["tool.status"] = "ok"
            if name in CACHEABLE:
                _TOOL_CACHE[key] = result
            return name, result
        except asyncio.TimeoutError:
            s["attrs"]["tool.status"] = "timeout"
            return name, {"error": "timeout", "elapsed_s": TIMEOUT_S,
                          "fallback_label": "unknown"}
```

### 4.3 Repeat-task scenario

The point of cache is repeat work, so run the **same task three times in a row** in one process. Note: we instantiate one `Tracer` outside the loop so all three runs append to the same JSONL file (otherwise `Tracer.__init__` truncates it on each run and you lose the cold-run spans).

```python
if __name__ == "__main__":
    tracer = Tracer("traces/cached.jsonl")
    times = []
    for run in range(3):
        llm = MockLLM(PARALLEL_SCRIPT)         # fresh script, same args
        t0 = time.perf_counter()
        asyncio.run(run_agent(
            llm, "Classify and summarize the three docs in ./data", tracer))
        times.append(time.perf_counter() - t0)
        print(f"run {run+1}: {times[-1]:.2f} s")
```

Expected: run 1 ≈ same as Part 2; runs 2 and 3 should be noticeably faster — LLM time dominates, because every tool call is now a cache hit (you can confirm by grepping `tool.cache`: `"hit"` in the trace).

```bash
python3 agent_p4.py
grep '"tool.cache": "hit"'  traces/cached.jsonl | wc -l
grep '"tool.cache": "miss"' traces/cached.jsonl | wc -l
```

### 4.4 Measure

| Run      | Wall time (s) | Tool cache hits | Tool cache misses |
| -------- | ------------- | --------------- | ----------------- |
| 1 (cold) | <br />        | <br />          | <br />            |
| 2 (warm) | <br />        | <br />          | <br />            |
| 3 (warm) | <br />        | <br />          | <br />            |

> **Deliverable for Part 4 (`reports/perf_report.md`):**
>
> 1. The 3×3 table above.
> 2. One paragraph: *"What would go wrong if you also cached* *`classify_flaky`?"* (Hint: think about what cache hits do to the *distribution* of observed latencies — and to the trace patterns from §3.)
> 3. One sentence: *"Which term in the latency identity does cache attack here?"* — and check it against your numbers.

---

## Final Deliverable: `perf_report.md`

Structure:

```
# Lab F Report

Name: ...
Date: ...

## Part 1 — Profile
- Latency identity table (LLM / tool / orchestration / total)
- Trace pattern this matches: ...
- Predicted dominant lever: ...
- Waterfall: traces/baseline.jsonl (attached) or screenshot

## Part 2 — Parallelize
- 5×2 wall-time table + mean speedup
- Amdahl ceiling = ... ; achieved ... × ; gap explained by ...
- Why p99 didn't move much: ...

## Part 3 — Tame the tail
- p50 / p95 / p99 before vs after
- Diagnosis of p50 / p99 movement
- Open question on fallback correctness

## Part 4 — Cache
- 3-run wall-time table + cache hit/miss counts
- Risks of caching a stochastic tool
- Latency identity term attacked

## Synthesis (1 paragraph)
- Which lever helped most for THIS task, and why?
- Would the answer change for a task that was 80% LLM time? 80% one slow API?
```

---

## Grading Rubric

| Criterion                                                    | Points |
| ------------------------------------------------------------ | ------ |
| Part 1 — instrumentation correct, OTel attribute names match | 20     |
| Part 1 — diagnosis paragraph identifies dominant term        | 10     |
| Part 2 — parallel dispatch works, traces show overlap        | 15     |
| Part 2 — speedup measured + Amdahl analysis                  | 10     |
| Part 3 — timeout + structured fallback (no exceptions)       | 15     |
| Part 3 — p50 / p99 before-vs-after with discussion           | 10     |
| Part 4 — cache hits visible in trace; correctness preserved  | 10     |
| Synthesis paragraph                                          | 10     |

**Bonus (+10)** — render waterfalls as PNGs (matplotlib `barh`) and embed them in the report.

**Bonus (+10)** — add a tiny load test: 10 concurrent agent tasks via `asyncio.gather`, plot the p50 / p99 of *task wall time under load* with vs without timeout. (This previews the multi-tenancy question from §2.)

---

## Submission Checklist

* [ ] `agent_p1.py`, `agent_p2.py`, `agent_p3.py`, `agent_p4.py`

* [ ] `tracer.py`, `waterfall.py`

* [ ] `traces/baseline.jsonl`, `traces/parallel.jsonl`, `traces/flaky_with_timeout.jsonl`, `traces/cached.jsonl`

* [ ] `reports/perf_report.md`

* [ ] (Optional) `reports/figures/*.png` waterfall renders

