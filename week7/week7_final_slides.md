---
marp: true
theme: default
paginate: true
style: |
  section {
    font-size: 24px;
    padding: 30px;
  }
  h1 {
    font-size: 36px;
    margin-bottom: 0.3em;
  }
  h2 {
    font-size: 28px;
    margin-bottom: 0.3em;
  }
  pre {
    font-size: 18px;
    padding: 10px;
    margin: 8px 0;
  }
  code {
    font-size: 18px;
  }
  table {
    font-size: 20px;
    margin: 8px 0;
  }
  th, td {
    padding: 4px 12px;
  }
  ul, ol {
    margin: 8px 0;
    line-height: 1.4;
  }
  li {
    margin: 4px 0;
  }
  p {
    margin: 8px 0;
    line-height: 1.4;
  }
  blockquote {
    margin: 8px 0;
    padding: 8px 16px;
    font-size: 22px;
  }
  img {
    max-height: 400px;
  }
  section.lead h1 {
    font-size: 42px;
  }
  section.small-table table {
    font-size: 18px;
  }
  section.small-code pre {
    font-size: 16px;
  }
  section.compact ul, section.compact ol {
    font-size: 22px;
    line-height: 1.3;
  }
---

<!-- _class: lead -->

# Week 7: From Container to Pod to Cluster
## Understanding Kubernetes through the myRAM system on Google Cloud

> One running system, one consistent story:
> **code → image → container → Pod → Deployment → Service → cluster control loop**

---

# Why this deck is reorganized

The original version had a real problem:

- too many concepts were presented as parallel facts
- the core thread was hard to see
- it was not easy to teach from a single mental model

This new version uses **one concrete system** as the anchor:

- **myRAM**, a research-agent platform running on **GKE Autopilot**
- with **control-plane services**, **dynamic sandbox Pods**, and **GPU workloads**

---

# The core teaching question

When students look at a cloud-native system, they should be able to answer:

1. **Why did we package this into containers?**
2. **Why is one container not enough, so we need Pods and Kubernetes objects?**
3. **How does Kubernetes actually keep the system running under failure and load?**

If they can answer those three questions, the chapter is coherent.

---

# The one-slide mental model

```text
Application code
    ↓
Container image
    ↓
Container runtime on one node
    ↓
Pod = the unit Kubernetes actually manages
    ↓
Deployment / Service / PVC / RBAC
    ↓
Scheduler + kubelet + control loops
    ↓
A real cloud system running on many machines
```

**Teaching thesis:** Kubernetes is not magic.
It is a structured control layer above the Linux mechanisms we already studied.

---

# Running case: what is myRAM?

myRAM is a research-agent system with two different kinds of work:

- a **control plane** written in **Node.js / TypeScript**
- user-facing **sandbox environments** that run Python / ML code
- some sandboxes need only CPU
- some sandboxes need **GPU**
- the whole system runs on **Google Cloud Platform**, on **GKE Autopilot**

This makes it a very good teaching case because it needs:

- packaging
- isolation
- scheduling
- storage
- identity
- networking

---

# What makes this case pedagogically strong?

It is not a toy system.

It contains many patterns students will later see in real jobs:

- multi-stage image builds
- sidecar containers
- init containers
- dynamic Pod creation through the Kubernetes API
- CPU vs GPU scheduling
- persistent volumes
- cloud identity and RBAC
- rollout and failure recovery

So instead of teaching K8s as vocabulary, we teach it as a **design response to real requirements**.

---

# Roadmap

| Section | Main question | Anchor in myRAM |
|---|---|---|
| I. Container layer | Why package code this way? | image family, multi-stage builds |
| II. Pod layer | Why is one container not enough? | web + proxy + init + sandbox Pods |
| III. K8s object layer | How does desired state become running state? | Deployment, Service, probes |
| IV. Scheduling layer | How are resources chosen and enforced? | CPU/GPU sandboxes on Autopilot |
| V. State and security | What must persist, and who is allowed to act? | PVC, Cloud SQL, GCS, RBAC |
| VI. Operations layer | How do we deploy, debug, and teach it? | rollout flow, failure modes, commands |

---

<!-- _class: lead -->

# Section I
## From application code to container images
---

# Start from the system requirement

myRAM must do two very different jobs well:

- run a long-lived **web/control service**
- create short-lived **isolated compute sandboxes**
- support both **CPU-only** and **GPU-backed** execution
- ship code reproducibly across machines
- avoid "works on my laptop" deployment drift

That is exactly the kind of problem where containers become the natural first step.

---

# Why containers are the right first abstraction

A plain process is too weak for deployment.
A full VM is often too heavy for fast, frequent packaging.

Containers give a practical middle point:

- package app + dependencies + filesystem view together
- reuse the host kernel
- isolate through namespaces and cgroups
- start quickly
- move the same image across nodes

This is why the lecture should begin from **containerization**, not directly from Kubernetes.

---

# Container = packaged process, not a tiny VM

| Question | Container answer |
|---|---|
| CPU and memory isolation? | cgroups |
| process / PID isolation? | namespaces |
| filesystem view? | mount namespace + OverlayFS |
| own kernel? | **No** |
| startup speed? | usually much faster than a VM |

**Key sentence for class:**
A container is still a Linux process tree, but wrapped in isolation and packaged with an image.

---

# The myRAM image family

```text
node:24-bookworm-slim
  └── myram-control-base
        └── myram-control

nvidia/cuda:12.8.1-runtime-ubuntu24.04
  ├── myram-batch-runner
  └── myram-sandbox
        ├── myram-sandbox-cpu
        └── myram-sandbox-gpu
```

This image family already tells students something important:

- different workloads need different runtime environments
- containers are not just for "one web app"
- image layering is an engineering decision, not just a syntax trick

---

# Why split the images into layers?

The control-plane stack separates:

- a **base image** with Python / ML runtime pieces that rarely change
- an **application image** with the frequently changing TypeScript code

The sandbox stack separates:

- a common runtime image
- specialized CPU and GPU images above it

This improves:

- build speed
- cache reuse
- image distribution time
- operational clarity
- attack-surface control

---

# myRAM control image: multi-stage build pattern

```dockerfile
FROM node:24-bookworm-slim AS build
COPY package*.json ./
RUN npm ci
COPY src/ tsconfig.json ./
RUN npm run build
RUN npm prune --omit=dev

FROM ${MYRAM_CONTROL_BASE}
COPY --from=build /app/node_modules ./node_modules
COPY --from=build /app/dist ./dist
ENV NODE_ENV=production
CMD ["node", "dist/bootstrap.js", "--host", "0.0.0.0", "--port", "8080"]
```

**Teaching point:** build tools and dev dependencies can exist in the build stage but stay out of the final runtime image.

---

# Why multi-stage build matters conceptually

Students often think Dockerfile is only about convenience.
It is actually about **separating build-time state from run-time state**.

That gives us:

- smaller final images
- less unnecessary software in production
- lower transfer cost to cluster nodes
- less security exposure
- clearer runtime behavior

This is a filesystem and deployment idea, not just a CI idea.

---

# OverlayFS intuition behind the image design

```text
read-only base layers
    +
read-only application layer
    +
one writable container layer at runtime
```

For teaching:

- the image is mostly **read-only, shareable state**
- the running container adds a **temporary writable layer**
- if the container dies, that writable layer is usually disposable

This prepares students for the later distinction between:

- container filesystem state
- persistent storage state

---

# The sandbox image is a runtime service, not just a shell

The sandbox image contains a **FastAPI server** that exposes operations such as:

- execute commands
- upload code or data
- stream logs
- abort work

That is a strong real-world teaching point:

- the control plane does **not** need to `exec` into the container manually
- the sandbox itself exposes a controlled runtime interface
- container lifecycle and application protocol are intentionally separated

---

# What `docker run` still means under the hood

```text
docker / containerd / runc
    ↓
clone() + namespaces
mount / pivot_root
cgroup configuration
execve()
```

This is where Week 5 still lives.

So when we later say:

- "Kubernetes started a Pod"

what really happened at the bottom is still:

- runtime calls into the Linux kernel

---

# What containers solve, and what they do not solve

| Containers solve | Containers do not solve |
|---|---|
| reproducible packaging | multi-node placement |
| environment consistency | self-healing across machines |
| process isolation | stable service identity |
| fast startup | cluster-wide scheduling |
| portable runtime images | dynamic policy and control loops |

This is the exact bridge we need before introducing Kubernetes.

---

# Teaching checkpoint for this section

Students should now be able to say:

> myRAM uses containers because we need reproducible, isolated runtime environments for a control service and for user sandboxes.

And they should also see the limit:

> containers alone explain packaging and isolation, but not orchestration.

---

<!-- _class: lead -->

# Section II
## From one container to one Pod
---

# Why Kubernetes manages Pods instead of single containers

A Pod is Kubernetes saying:

- some containers must start and live together
- some containers must share one network identity
- some containers should share volumes
- the scheduler should place them as **one unit**

So a Pod is not just a wrapper.
It is the smallest unit of **co-scheduling** and **shared context**.

---

# Pod = a small execution boundary

Inside one Pod, containers typically share:

- one **network namespace**
- one Pod IP address
- one loopback interface
- selected shared volumes

They do **not** have to be identical in role.
A Pod can contain:

- the main app container
- a sidecar
- an init container before the main containers start

---

# myRAM control-plane Pod: why one container is not enough

The control-plane Pod contains three distinct roles:

1. **init container**: prepare directories under `/var/lib/ram/workspaces`
2. **web container**: run the Node.js application on port 8080
3. **cloud-sql-proxy sidecar**: expose database connectivity on port 5432

This is a perfect classroom example because each container has a different job, but the Pod gives them one coordinated execution unit.

---

# Control-plane Pod anatomy

```text
Pod: myram-control-plane
  ├── init-dirs            (runs first, then exits)
  ├── web                  (main application container)
  └── cloud-sql-proxy      (sidecar container)

Shared by the Pod:
  - one Pod IP
  - one localhost
  - mounted volumes
```

**Instructor line:** the Pod is closer to a "process group with shared context" than to a single Linux process.

---

# Why the sidecar belongs in the same Pod

The web container talks to the database proxy at:

- `localhost:5432`

That only works cleanly because both containers share the same Pod network namespace.

Benefits of the sidecar design here:

- local communication path is simple
- the proxy lifecycle follows the app lifecycle
- security boundary is smaller than exposing a separate service hop
- operational dependency is explicit

---

# Shared network namespace: the important intuition

```text
Outside the Pod:
  Service / other clients
        ↓
     Pod IP

Inside the Pod:
  web container  ---- localhost:5432 ----> cloud-sql-proxy
```

For students, the important lesson is:

- a Pod is where Kubernetes says "these containers are one networked thing"

That is why a Pod is more meaningful than an individual container in many real systems.

---

# Init container = startup logic with clear boundaries

The `init-dirs` container exists only to prepare state before the main app starts.

Why this is better than folding everything into application startup code:

- startup dependencies become visible in YAML
- failure happens early and clearly
- one-time preparation logic stays separate from steady-state logic
- students see that Kubernetes supports a startup phase, not only a run phase

---

# Dynamic sandbox Pods: a second Pod pattern in the same system

myRAM also creates sandbox Pods dynamically at runtime.

These Pods are different from the control-plane Pod:

- they are not long-lived service replicas
- they are created on demand for user work
- they can be CPU-only or GPU-backed
- they represent isolated execution environments

So one system already gives us **two major Kubernetes usage modes**.

---

# Sandbox templates: CPU and GPU are different scheduling requests

CPU sandbox template:

- request / limit around `4 CPU`, `16Gi` memory

GPU sandbox template:

- request / limit around `8 CPU`, `32Gi` memory
- request `nvidia.com/gpu: 1`
- tolerate GPU taints
- select nodes with the expected accelerator label

This is where Pod spec becomes scheduling policy.

---

# Why the sandbox Pod is a good OS teaching object

The sandbox Pod makes several abstractions concrete at once:

- **container**: packaged runtime for Python / ML work
- **Pod**: the K8s unit that gets created and scheduled
- **cgroup boundary**: CPU / memory / GPU allocation applies here
- **security boundary**: user code runs inside this isolated environment

It is much easier to teach K8s when students can imagine a real per-user compute Pod.

---

# Warm pool: pre-created Pods to hide cold-start cost

myRAM uses a warm-pool idea:

- keep some CPU sandboxes ready
- keep GPU sandboxes at zero idle replicas unless needed

Why?

- CPU sandboxes are cheaper to keep warm
- GPU nodes are expensive
- GPU node startup can be slow

This connects directly to classic systems trade-offs:

- latency vs cost
- pre-allocation vs on-demand allocation

---

# Container vs Pod vs Deployment

| Abstraction | What it means in this lecture |
|---|---|
| Container | one packaged runtime |
| Pod | one co-scheduled execution unit |
| Deployment | one controller that keeps desired replicas alive |

Students often mix these three up.
This is the moment to separate them clearly.

---

# Teaching checkpoint for this section

Students should now be able to explain:

- why the control-plane service is a **Pod**, not just a container
- why a sidecar can safely live next to the web container
- why a sandbox is also a **Pod**, even if it contains only one main container

Once that is clear, we can move to Kubernetes objects and control loops.

---

<!-- _class: lead -->

# Section III
## Kubernetes objects as the distributed OS layer
---

# Kubernetes begins where containers stop

Containers explain:

- how code is packaged
- how a runtime is isolated on one node

Kubernetes explains:

- how desired state is declared
- how work is placed on nodes
- how services are recovered after failure
- how stable identity exists even when Pods come and go

So the clean transition is:

> Docker gives us the runtime unit. Kubernetes gives us the control system.

---

# Imperative vs declarative control

| Imperative | Declarative |
|---|---|
| run this container now | keep this service alive continuously |
| one command, one action | control loop checks and repairs |
| human drives recovery | controller drives recovery |
| node-local view | cluster-wide desired state |

This is the single most important mindset shift for students.

---

# The myRAM object map

| OS-flavored idea | Kubernetes object | myRAM example |
|---|---|---|
| running unit | Pod | control-plane Pod, sandbox Pod |
| service manager | Deployment | `myram-control-plane` |
| stable network endpoint | Service | ClusterIP / LoadBalancer for web service |
| durable disk attachment | PVC | `myram-state` mounted at `/var/lib/ram` |
| user / permission boundary | ServiceAccount + RBAC | control plane can create sandbox Pods |
| cluster scheduler | kube-scheduler + Autopilot capacity | place CPU / GPU Pods |

This table gives the lecture a stable vocabulary.

---

# The smallest useful set of K8s objects in this system

To make the control service work, myRAM needs at least:

- a **Deployment** for the control-plane Pod
- a **Service** for stable access
- a **PVC** for workspace state
- a **ServiceAccount** and **RBAC** rules
- optional **LoadBalancer / Ingress** depending on exposure mode

This is a good correction to a common misconception:

> Kubernetes is not "just Pods".

---

# The full path from YAML to running processes

```text
kubectl apply
    ↓
API server stores desired object
    ↓
controllers notice desired state
    ↓
scheduler chooses a node for each Pod
    ↓
kubelet on that node calls container runtime
    ↓
Linux kernel runs the actual processes
```

This should be repeated often in class.
It connects the student-visible YAML to the kernel-visible runtime.

---

# Deployment: why the control plane should be declared, not hand-started

The control-plane service is a long-lived component.
That means we want Kubernetes to own these responsibilities:

- keep the desired replica count
- recreate the Pod if it crashes
- coordinate image updates
- attach the volume and network identity correctly
- report rollout state

That is why the control service belongs in a **Deployment**.

---

# Why myRAM uses `Recreate`, not `RollingUpdate`

The control-plane Pod mounts a PVC with **ReadWriteOnce** semantics.

That means:

- at one time, one node can mount it
- an old Pod and a new Pod cannot safely overlap on different nodes

So the update strategy is:

- stop old Pod
- start new Pod

This is an excellent teaching example because it shows that rollout strategy depends on the **storage model**, not just on preference.

---

# Readiness and liveness are different questions

The web container defines probes such as:

- `/readyz` for **readiness**
- `/healthz` for **liveness**

Students should distinguish them precisely:

- **readiness** = may this Pod receive traffic yet?
- **liveness** = should Kubernetes restart this container because it is unhealthy?

That distinction is crucial for real service operation.

---

# Service: stable name over unstable Pods

Pods are ephemeral.
Their IP addresses are not the right thing to depend on.

A Service gives:

- a stable DNS name
- a stable virtual IP
- routing to healthy matching Pods

In myRAM this means:

- clients use the service endpoint
- the Pod may be recreated underneath
- connectivity remains stable at the object level

---

# Staging exposure path in this system

```text
Internet
   ↓
LoadBalancer
   ↓
ClusterIP Service
   ↓
control-plane Pod
   ↓
web container :8080
```

Possible later production path:

```text
Internet → Ingress / HTTPS / auth layer → Service → Pod
```

This lets students see that Kubernetes networking objects build layers of indirection deliberately.

---

# What Kubernetes does not do for you automatically

Kubernetes gives structure, but not perfect design.
You still must decide:

- what belongs in one Pod
- what state is ephemeral vs durable
- which permissions the control plane gets
- what update strategy is safe
- when to create CPU vs GPU environments

This is useful pedagogically because it keeps students from treating YAML as magic.

---

# Teaching checkpoint for this section

Students should now be able to say:

> a Deployment keeps the control-plane Pod alive, a Service gives it stable reachability, and the kubelet ultimately realizes that desired state on a node.

Now we can ask the next question:

> how does Kubernetes choose the node and enforce resources?

---

<!-- _class: lead -->

# Section IV
## Scheduling and resource management in a real cluster
---

# Resource management starts in the Pod spec

In myRAM, the Pod spec is not just configuration text.
It is a resource claim.

Examples from the system:

- control plane: moderate CPU and memory, always on
- CPU sandbox: `4 CPU`, `16Gi`
- GPU sandbox: `8 CPU`, `32Gi`, `nvidia.com/gpu: 1`

The scheduler and kubelet both depend on this declaration, but in different ways.

---

# Requests vs limits: the simplest important distinction

| Field | Main role |
|---|---|
| `requests` | scheduling and guaranteed reservation logic |
| `limits` | runtime enforcement boundary |

Teaching sentence:

- **request** answers: "may this Pod be placed here?"
- **limit** answers: "how far may this Pod go after it is running?"

Students who understand only that distinction already understand a lot.

---

# Scheduler view: filter → score → bind

```text
Pod arrives
   ↓
Filter: which nodes can run it at all?
   ↓
Score: among those, which node is preferable?
   ↓
Bind: record the chosen node
```

This is the cleanest way to teach scheduling.
Do not start from plugin names.
Start from the decision shape.

---

# myRAM CPU sandbox: what the scheduler cares about

For a CPU sandbox, the scheduler mainly asks:

- is there enough free CPU request capacity?
- is there enough free memory request capacity?
- are there policy constraints from labels or taints?

If yes, the Pod can be placed.
If no, it stays **Pending**.

That gives students a very concrete way to interpret scheduling outcomes.

---

# myRAM GPU sandbox: what makes it harder

GPU Pods add more constraints:

- request an **extended resource**: `nvidia.com/gpu: 1`
- tolerate GPU node taints
- select the right accelerator type
- fit larger CPU and memory requests too

So a GPU Pod is a good teaching case for multi-dimensional scheduling:

- CPU
- memory
- accelerator type
- node policy

---

# Why GPU scheduling is different from CPU scheduling

| CPU / memory | GPU |
|---|---|
| naturally overcommitted in many systems | usually treated as a discrete scarce device |
| fractional requests are normal | whole-device requests are common |
| cgroup enforcement is standard | device-plugin / driver path matters |
| many nodes have it | only some nodes have it |

This difference helps students see why cluster scheduling is richer than local CPU scheduling.

---

# Autopilot changes who manages capacity, not the abstraction

In GKE Autopilot:

- Google manages more of the node provisioning details
- the user still expresses CPU / memory / GPU intent through Pod specs
- scheduling still depends on requests, constraints, and cluster policy

So managed Kubernetes changes **operations ownership**.
It does not remove the need to understand the core abstractions.

---

# Why Pods become Pending in practice

For this system, a Pod can remain Pending because:

- request does not fit on any current node
- GPU node of the right type is unavailable
- taints are not tolerated
- PVC or other attachment constraints delay placement
- the cluster is still provisioning capacity

This is a better teaching list than abstract scheduler jargon.

---

# Warm pool is a scheduling policy above raw placement

Warm pool says:

- keep some capacity pre-realized as ready Pods
- trade idle cost for lower user-visible startup latency

For myRAM:

- CPU warm pool makes sense because the cost is moderate
- GPU warm pool is usually zero because idle GPU capacity is expensive

This is an excellent place to connect systems design to cloud economics.

---

# The real trade-off: latency, utilization, and cost

```text
More warm capacity
  → lower cold-start latency
  → higher idle cost

Less warm capacity
  → lower idle cost
  → higher user wait time when demand spikes
```

This is not uniquely a Kubernetes issue.
It is a classic systems provisioning problem, now expressed through Pods.

---

# Enforcement still returns to Linux on the node

Even in a managed cluster, once a Pod is placed:

- kubelet configures the runtime
- the runtime creates containers
- Linux enforces CPU and memory boundaries

So the whole story is layered:

- Kubernetes decides
- kubelet translates
- Linux enforces

That sentence is worth repeating several times during the lecture.

---

# A useful classroom debugging loop

If a sandbox Pod is slow or unschedulable, ask in this order:

1. Is the Pod **Pending** or **Running**?
2. If Pending, which placement constraint failed?
3. If Running, is the bottleneck CPU, memory, GPU, I/O, or network?
4. Is the issue scheduling, or is it enforcement after placement?

This gives students an actionable debugging discipline.

---

# Teaching checkpoint for this section

Students should now be able to explain:

- why requests and limits are not the same thing
- why GPU Pods are harder to place than CPU Pods
- why warm pools are a latency/cost decision
- why Kubernetes scheduling still ends in Linux enforcement on the node

---

<!-- _class: lead -->

# Section V
## State, storage, identity, and security
---

# Container filesystems are not where important state should live

A running container usually writes into a temporary writable layer.
That is fine for:

- transient logs
- temporary files
- process-local scratch data

It is not fine for durable system state.

That is why a real service always forces us to teach storage separately from containers.

---

# myRAM uses three persistence layers for different jobs

| Storage layer | Role in the system | Why it exists |
|---|---|---|
| PVC | workspace files and project data | persistent filesystem state across Pod restarts |
| Cloud SQL | metadata and records | transactional structured state |
| GCS bucket | experiment artifacts and logs | durable object storage |

This table is important because it prevents students from thinking "Kubernetes storage" is one thing.

---

# PVC: durable filesystem state for the control plane

The system mounts a PVC such as `myram-state` at `/var/lib/ram`.

Why this matters:

- Pod restarts should not erase workspace files
- application code can keep using a filesystem path
- Kubernetes can reattach durable storage to a replacement Pod

This is the main bridge from ephemeral containers to persistent service behavior.

---

# Why `ReadWriteOnce` changes operations

The PVC uses `ReadWriteOnce` semantics.
That means:

- the volume is not a general shared filesystem
- one writer placement model dominates
- rollout strategy must respect attachment constraints

So storage choice shapes:

- update strategy
- failover behavior
- scheduling flexibility

Students need to see that storage is a control-plane decision too.

---

# Database access through a sidecar proxy

In myRAM, the application does not talk to Cloud SQL through a hard-coded external database path alone.
It uses a **cloud-sql-proxy** sidecar.

Benefits:

- simpler local connection model for the app
- proxy lifecycle follows the Pod lifecycle
- database connectivity logic is separated from app logic

This is another reason the Pod abstraction is pedagogically powerful.

---

# Identity in Kubernetes is workload identity, not just passwords

The control-plane Pod needs to call cloud services.
In this system that includes capabilities such as:

- calling LLM-related cloud APIs
- creating cloud batch jobs
- accessing Cloud SQL
- publishing / subscribing to messaging systems
- using object storage

The important lesson is:

> a Pod has an identity.

---

# Workload Identity: mapping Pod identity to cloud identity

```text
Kubernetes ServiceAccount
        ↓
Workload Identity binding
        ↓
Google IAM service account
        ↓
Cloud API permissions
```

This is much better than baking static secrets into container images.

Students should understand this as the cloud-native replacement for:

- shipping credentials inside the filesystem
- stuffing long-lived keys into environment variables

---

# RBAC: what the control plane is allowed to do inside Kubernetes

myRAM gives its control plane permissions to interact with objects such as:

- Pods
- Pod logs
- exec-related interfaces
- Services
- ConfigMaps

with verbs such as:

- get
- list
- watch
- create
- delete

This is the internal authorization story of the cluster.

---

# Security boundary: what if a sandbox is compromised?

This is the right question for an OS course.

A compromised sandbox may still be limited by:

- container / namespace isolation
- Pod-level resource boundaries
- RBAC boundaries
- cloud identity scoping
- network design

But it may still have dangerous power inside its own boundary.

So the lesson is not "containers make it safe".
The lesson is **defense in depth**.

---

# The practical security message

For students, the clean takeaway is:

- container image choice matters
- Pod composition matters
- ServiceAccount and RBAC matter
- cloud identity mapping matters
- storage and network boundaries matter

Security in Kubernetes is never one switch.
It is a stack of mechanisms.

---

# Teaching checkpoint for this section

Students should now be able to explain:

- why durable state cannot live only in the container writable layer
- why a Pod identity is a first-class systems concept
- why RBAC and Workload Identity are core operating mechanisms in modern cloud systems

---

<!-- _class: lead -->

# Section VI
## End-to-end operation: deploy, debug, and teach the system
---

# End-to-end deployment flow

```text
Developer changes code
    ↓
Build image
    ↓
Push image to Artifact Registry
    ↓
Update Kubernetes object to a specific image tag
    ↓
Deployment creates replacement Pod
    ↓
Init container runs
    ↓
Readiness probe passes
    ↓
Service sends traffic to the new Pod
```

This is the operational version of the earlier architectural story.

---

# Why exact image tags matter in practice

myRAM uses timestamped image tags such as:

- `staging-20260329-143052`

Instead of relying on `:latest`.

Teaching reason:

- the deployment should point to an exact artifact
- rollouts should be auditable and reproducible
- image caching behavior can otherwise create confusing results

This is a very realistic cloud operations lesson.

---

# The rollout command students can actually understand

```bash
kubectl set image deployment/myram-control-plane \
  web=us-central1-docker.pkg.dev/.../myram-control:staging-20260329-143052
```

This one command contains the full chain:

- new image artifact
- Deployment update
- new Pod realization
- probe-gated traffic cutover

It is a good slide because it is concrete but still conceptually rich.

---

# End-to-end user request flow in myRAM

```text
User asks for a research task
    ↓
control-plane service decides what environment is needed
    ↓
if needed, control plane creates a sandbox Pod through the K8s API
    ↓
scheduler places the Pod
    ↓
kubelet starts the container runtime on the chosen node
    ↓
sandbox FastAPI server receives code / commands
    ↓
results flow back to the control plane and durable storage
```

That is a complete cloud-native control loop built from the abstractions in this lecture.

---

# One system, many OS questions

| Operational question | Where the answer lives |
|---|---|
| Why did this service restart? | Deployment / kubelet / probes |
| Why is this Pod Pending? | scheduler constraints and events |
| Why did data survive restart? | PVC / database / object storage |
| Why can this Pod call cloud APIs? | ServiceAccount + Workload Identity |
| Why can one component create sandbox Pods? | RBAC |

This table is useful near the end because it ties the abstractions back to debugging questions.

---

# Common failure stories worth teaching explicitly

1. **Pod crash**
   - Deployment creates a replacement
2. **new image is broken**
   - readiness fails, rollout should not become healthy
3. **PVC prevents overlap**
   - rollout strategy must be Recreate
4. **GPU sandbox is slow to appear**
   - cluster may need GPU capacity before scheduling succeeds
5. **sandbox privilege is too broad**
   - RBAC / identity review becomes a design issue

These are much easier to teach than abstract fault scenarios.

---

# The minimum command set for teaching and debugging

```bash
kubectl get pods -o wide
kubectl describe pod <pod>
kubectl get events --sort-by=.lastTimestamp
kubectl logs <pod> -c web
kubectl logs <pod> -c cloud-sql-proxy
kubectl get svc
kubectl get deploy
```

These commands map directly to the objects students saw in the lecture.

---

# How to teach the lecture clearly

Recommended flow in class:

1. start with the **myRAM system diagram**
2. move from **container** to **Pod**
3. then introduce **Deployment / Service / PVC / RBAC**
4. only then discuss **scheduler and resource policy**
5. finish with **deployment flow and debugging**

This order works because each layer answers a question left open by the previous layer.

---

# A good board question to ask students

Suppose a user requests a GPU sandbox and waits a long time.
Where might the delay come from?

Possible answers students should now have:

- image pull time
- no ready warm Pod
- no matching GPU node yet
- taint / selector mismatch
- readiness not complete
- application startup inside the sandbox

This question checks whether they can reason across layers.

---

# The final synthesis

```text
Container answers: how do I package and isolate one runtime?
Pod answers: what must run together as one unit?
Deployment answers: what should keep existing?
Service answers: how do I reach it stably?
PVC answers: what state must survive?
RBAC / identity answer: who is allowed to act?
Scheduler answers: where should it run?
Linux answers: how are resources finally enforced?
```

If students can retell this chain, the lecture succeeded.

---

# Summary

1. **Use one real system as the anchor.** myRAM makes the abstractions concrete.
2. **Container first, then Pod, then Kubernetes objects.** That order is easier to teach.
3. **Pods exist because some containers must share one execution context.**
4. **Kubernetes adds desired state, recovery, placement, and stable identity.**
5. **Scheduling is policy; Linux enforcement is still the bottom layer.**
6. **State, identity, and security are core system design choices, not add-ons.**
7. **The full cloud story is operational only when build, rollout, and debugging are visible.**

---

# Discussion prompts

1. Why is a Pod a better abstraction than a single container for the myRAM control plane?
2. Why does `ReadWriteOnce` push the system toward `Recreate` rather than `RollingUpdate`?
3. Why should GPU sandboxes usually not stay warm all the time?
4. Why is Workload Identity a systems concept, not just a cloud product feature?
5. Which parts of this whole stack are Kubernetes decisions, and which parts are still Linux decisions?

---

<!-- _class: lead -->

# Optional appendix
## Minimal control-plane internals for instructor use

---

# If you want one slide on the K8s control plane

```text
API server = entry point and object interface
scheduler  = chooses a node for unscheduled Pods
controllers = keep desired state true over time
kubelet    = node agent that realizes Pods locally
etcd       = durable store for cluster object state
```

Use this only after the main story is clear.
The main lecture should not start here.

---

# The one-sentence distributed OS analogy

A traditional OS manages processes on one machine.
Kubernetes manages Pods across many machines by keeping a shared desired state and continuously reconciling the actual cluster back toward it.

That analogy is useful.
But in this chapter it should support the story, not replace it.

---

# Suggested reference slide

- Kubernetes documentation: Pods, Deployments, Services, scheduling, probes
- GKE documentation: Autopilot, Workload Identity, GPU scheduling
- OCI / container runtime documentation for image and runtime basics
- Internal myRAM notes in `week7/myRam.md`

---

# End

## Teaching slogan

**Do not teach Kubernetes as vocabulary. Teach it as one system climbing layer by layer from process to cloud service.**
