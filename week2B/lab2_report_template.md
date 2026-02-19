# Lab 2 Report (Route B): PagerUSE — Oncall Debugging with USE

**Name:**

**Scenario ID:** (easy_memory_major_faults / medium_multi_cause / hard_misleading_network)

---

## 1. Symptom (what page woke you up)

- Alert(s):
- Impact scope: (which service, what % requests)
- Timeline: (when started; any relevant change events)

## 2. Initial hypotheses (USE first, mechanism later)

Write 2–3 bullet hypotheses. At least one must be an *alternative* you will later rule out.

- Hypothesis A (resource + mechanism guess):
- Hypothesis B (resource + mechanism guess):
- Why you picked these first (from alerts/context):

## 3. Investigation transcript (key commands + key lines)

You do **not** need to paste full outputs. Paste the important lines and explain why they matter.

### 3.1 CPU

Commands + key lines:

```
<command>
<key output lines>
```

Interpretation:

### 3.2 Memory

Commands + key lines:

```
<command>
<key output lines>
```

Interpretation:

### 3.3 Disk / Storage

Commands + key lines:

```
<command>
<key output lines>
```

Interpretation:

### 3.4 Network (only if relevant)

Commands + key lines:

```
<command>
<key output lines>
```

Interpretation:

## 4. Evidence chain (must include ≥ 2 independent signals)

Write a short evidence chain:

> symptom → resource → saturation/waiting signal → mechanism → why p99 (not average)

- Signal 1:
- Signal 2:
- (Optional) Signal 3:

## 5. Mechanism explanation (1–2 paragraphs)

Explain the OS slow path you believe dominates p99:

- queueing / scheduler delay
- major faults / paging
- storage tail (fsync/writeback)
- retry amplification (symptom != root cause)

Use your evidence to justify causality.

## 6. Mitigation + verification plan

### Mitigation

- What would you change first (and why):

### What to verify next (at least 2)

- Verify #1:
- Verify #2:
- (Optional) Verify #3:

## 7. Reflection

- One thing you almost misdiagnosed (and what corrected you):
- One extra measurement you wish you had:
