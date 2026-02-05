# Advanced Operating Systems for the Cloud-Native & AI Era

## Syllabus

---

## General Course Information

| | |
|---|---|
| **Class Time** | 12:45 PM – 2:05 PM (80 min) |
| **Location** | Memorial Hall, Room 113 |
| **Instructor** | Dong Dai |
| **Office** | Fintech 416B |
| **Office Hours** | TBD |
| **TA Office Hours** | TBA |

If you want to send me an email regarding this course, please include the course number in the email subject.

---

## Teaching Assistant

**Minqiu Sun**
- Ph.D. Student, DIRLab Member
- Research: LLM Post-training and Agent System Design
- Office Hours: TBA

---

## Course Description

This course teaches **core operating system concepts** (processes, memory, scheduling, I/O, security) through **modern practical contexts** (containers, Kubernetes, eBPF, LLM workloads). Students learn to measure, explain, and debug real systems.

Unlike traditional OS courses that focus primarily on kernel internals, this course emphasizes:
- **Understanding mechanisms**: Why does this behavior happen?
- **Measurement skills**: How do we collect evidence?
- **Reproducibility**: Can someone else verify our results?

This is a graduate-level course suitable for PhD students, Master's students, and senior undergraduates with appropriate prerequisites.

---

## Course Objectives

By the end of this course, students will be able to:

1. **Explain** core OS concepts: process lifecycle, scheduling policies, virtual memory, file systems, and synchronization primitives
2. Use **perf and eBPF** to conduct reproducible experiments investigating p95/p99 latency or resource contention
3. Construct **isolation boundaries** using namespace, cgroup, seccomp, and capabilities — and explain the underlying mechanisms
4. Treat Kubernetes scheduling and resource management as a **cluster-level resource control plane**, and reproduce experiments demonstrating OOM, throttling, and eviction
5. Profile **LLM inference** as a server workload, analyzing concurrency, memory, and I/O
6. Build a **minimal agent sandbox** with least privilege, audit logging, and tool call isolation

---

## Prerequisites

**Required:**
- Ability to design, code, compile, and execute programs in **C/C++** on a Unix (Linux) machine
- Basic Linux command-line proficiency (shell, file permissions, process management)
- Familiarity with concurrency basics (threads, locks, condition variables or semaphores)

**Recommended:**
- Prior coursework in operating systems (undergraduate level)
- Experience with debugging tools (gdb, printf debugging)

Students with weaker backgrounds are welcome but should plan extra time for the first few weeks.

---

## Course Organization

### Two Sessions Per Week

| Session | Format | Duration | Content |
|---------|--------|----------|---------|
| **Session 1: Lecture** | Instructor-led | 80 min | OS concepts + modern context + case study + lab preview |
| **Session 2: Lab Workshop** | Hands-on | 80 min | Start lab + get help from instructor/TA |

### How It Fits Together

```
SESSION 1: LECTURE (in class)
  • I teach OS concepts, modern context, and case studies
  • In-class quiz/activity → Participation (20%)
                    ↓
SESSION 2: LAB WORKSHOP (in class)
  • You START the lab with me and TA available
  • You will NOT finish in class!
                    ↓
AFTER CLASS: Complete Lab Assignment
  • Continue working on the lab outside of class
  • Deadline: before next week's lecture
  • Lab submission → Labs (40%)
```

---

## Covered Topics

### Core OS Concepts
- Process lifecycle and context switch
- Threads and concurrency
- Scheduling policies (CFS, real-time, priority)
- Virtual memory management
- File systems and I/O
- Security and isolation mechanisms

### Modern Applications
- Performance measurement methodology
- Linux kernel observability (perf, eBPF, tracepoints)
- Container isolation (namespaces, cgroups)
- Kubernetes resource management
- Network data plane and latency
- Storage and tail latency
- LLM inference workloads
- AI agent runtime and sandboxing

---

## 12-Week Schedule

| Week | Topic | Lab |
|------|-------|-----|
| 1 | Course Introduction | Lab 0: Environment Setup |
| 2 | Performance Methodology | Lab 1: Quicksort Performance Analysis |
| 3 | Concurrency & Scheduling | Lab 2: User-Level Thread Library |
| 4 | Linux Scheduler Observability | Lab 3: Scheduling Latency (SchedLab) |
| 5 | Container Boundaries | Lab 4: Mini Container Runtime |
| 6 | Kubernetes Resource Management | Lab 5: K8s Resource Experiments |
| 7 | Network Data Plane | Lab 6: Network Latency Analysis |
| 8 | Storage & Tail Latency | Lab 7: Storage Tail Latency Investigation |
| 9 | Security & Policy | Lab 8: Threat Model and Minimal Defense |
| 10 | LLM Workloads | Lab (choice): LLM Inference Profiling |
| 11 | Agent Runtime | Lab E: Minimal Agent Sandbox |
| 12 | Final Defense | Reproduction Day |

---

## Assessment

Learning outcomes will be assessed through labs, a final project, and participation. **There are no closed-book exams.**

| Component | Weight | Description |
|-----------|--------|-------------|
| **Final Project** | 40% | System case study with milestones |
| **Labs** | 40% | ~8 lab assignments (~5% each) |
| **Participation** | 20% | In-class quizzes, activities, discussion |

### Labs (40%)

- **~8 labs** throughout the semester, each worth approximately 5%
- Labs have **tiered difficulty**: basic requirements (required) + advanced challenges (optional bonus)
- **Deadline:** Before the next week's lecture (exact time on each lab handout)
- **Grading criteria:** Reproducibility + explanation quality

### Final Project (40%)

- System case study: investigate a real system behavior
- Work individually or in pairs
- **4 milestones:**

| Week | Milestone | Deliverable |
|------|-----------|-------------|
| 2 | Proposal | 1-page problem statement + minimal reproduction |
| 6 | Checkpoint | Tracing evidence (eBPF or perf data) |
| 10 | Freeze | Clear metric + intervention plan |
| 12 | Defense | Presentation + peer reproduction |

### Participation (20%)

Participation is earned through **in-class activities during lectures**:
- In-class quizzes (unannounced, short)
- In-class activities and discussions
- Asking and answering questions
- Helping classmates

**You must attend lecture to earn participation credit.**

---

## Late Submission Policy

- You can submit **one assignment 3 days late** without any questions asked or penalty (first time only, use wisely!)
- For subsequent late submissions within 3 days, you must provide appropriate reasons (e.g., outstanding circumstances); otherwise, your submission will not be graded
- Submissions more than 3 days late will **not be accepted** without a University-approved excuse or prior permission

---

## Attendance Policy

- **Students are required to attend all classes**
- If you will miss classes for scheduled events (e.g., attending conferences), notify me at least **2 weeks in advance** with your advisor's confirmation
- A **doctor's note** is required for sick leave
- Each student is allowed **one unexcused absence** during the term

---

## Environment Requirements

- Students use their own machines with **VirtualBox VMs**
- Standard environment: **Ubuntu 22.04 or 24.04 LTS** in VM
- **Not supported:** WSL (limited kernel features), Docker (restricted BPF capabilities)
- Setup guide provided in course materials

---

## Course Materials

All course texts are available **free online**:

**Primary:**
- [Operating Systems: Three Easy Pieces](https://pages.cs.wisc.edu/~remzi/OSTEP/) by Remzi H. Arpaci-Dusseau and Andrea C. Arpaci-Dusseau
- [Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/)
- Brendan Gregg's [Systems Performance](https://www.brendangregg.com/systems-performance-2nd-edition-book.html) resources

**Additional:**
- [The Linux Command Line](https://linuxcommand.org/tlcl.php) by William Shotts
- [Dive into Systems](https://diveintosystems.org/) by Matthews, Newhall, and Webb
- Various papers and documentation (assigned per week)

---

## Policy on Academic Conduct

The assignments in this class should be performed **individually** unless otherwise indicated.

**What is allowed:**
- ✅ Discussing conceptual problems with other students
- ✅ Using generative AI (ChatGPT, Copilot, etc.) to facilitate learning
- ✅ Searching for documentation and examples online

**What is NOT allowed:**
- ❌ Copying code or specific solutions from other students
- ❌ Submitting work you do not understand

**The key rule:** If you cannot explain your code, you didn't write it.

Any evidence of academic dishonesty will be handled as stated in the Official Student Handbook of the University of Delaware. If you are in doubt regarding the requirements, please consult with course staff before you complete any requirement of this course.

---

## Statement on Inclusivity

In this class, I am committed to creating an **inclusive environment** in which all students are respected and valued. I will not tolerate disrespectful or exclusive language or behavior on the basis of age, ability, color/ethnicity/race, gender identity/expression, marital/parental status, military/veteran's status, national origin, political affiliation, religious/spiritual beliefs, sex, sexual orientation, socioeconomic status, or other visible or non-visible differences.

What a boring world this would be if everyone was the same race, culture, religion, gender, had the same abilities, held the same opinions, etc. Being grateful for the differences includes not just being respectful of those possessing those differences, but truly valuing what those differences bring to our classroom, to our learning experience, and to our lives.

---

## Student Diversity

This class includes PhD students, Master's students, and senior undergraduates. To accommodate this diversity:

1. **Lectures cover concepts clearly** — no assumed background beyond prerequisites
2. **Labs have tiered difficulty** — basic requirements + optional advanced challenges
3. **Final project scope is adjustable** — expectations calibrated to student level
4. **Questions are encouraged** — there are no dumb questions!

If you are struggling, please reach out early. We are here to help you succeed.

---

## Getting Help

- **During lab workshop:** Ask the instructor or TA
- **Office hours:** Come with specific questions
- **Email:** Include course number in subject line
- **Classmates:** Collaboration on concepts is encouraged

**Don't struggle alone.** Systems can be complex; asking for help is a sign of good engineering practice.
