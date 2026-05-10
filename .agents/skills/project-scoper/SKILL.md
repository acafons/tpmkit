---
name: project-scoper
description: Deeply investigates and scopes a project before design or implementation. Use when Codex should interview the user, inspect project context, research relevant upstream/domain material, fill every section of templates/project-scope-template.md, and create or update a requested scope output file with requirements, diagrams, constraints, risks, open questions, and deliverables. Do not use for implementation plans, code changes, architecture design documents, or general project summaries.
---

# Project Scoper

## Purpose

Use this skill to turn an early project idea into a complete scope document at a requested output path using `templates/project-scope-template.md`.

Define requirements and boundaries. Do not create a detailed implementation design unless the user explicitly asks for that.

## Required Inputs

Before writing a scope document, collect enough context to fill every template section with project-specific content.

Always identify the requested output path for the generated scope document before writing. If the user, agent, or surrounding task context does not provide an output path, ask where the scope document should be generated.

Always inspect:

- `templates/project-scope-template.md`
- any existing scope document at the requested output path
- relevant README, docs, source files, issue notes, or planning files in the workspace

When the project references external upstream APIs, standards, libraries, or repositories, research authoritative sources before making domain claims. Prefer official docs, specifications, and upstream repositories.

## Workflow

1. Identify the output path for the generated scope document.
2. Read the scope template and existing project context.
3. Identify missing information for each template section.
4. Ask the user a focused batch of questions.
5. Repeat questioning until enough information exists to produce a useful first scope.
6. Fill every section from `templates/project-scope-template.md`.
7. Write the completed scope to the requested output path.
8. Report the created or updated file and summarize unresolved assumptions or open questions.

Ask many questions, but keep each batch usable. Prefer 6-12 high-signal questions per round instead of one very long interrogation.

If the user cannot answer something yet, proceed by recording it under `Assumptions` or `Open Questions` rather than blocking forever.

## Questioning Guidance

Ask about:

- the problem being solved
- target users and stakeholders
- desired outcomes
- functional requirements
- non-functional requirements
- scope boundaries
- constraints
- assumptions
- success criteria
- risks
- expected diagrams
- concrete deliverables

Use `references/question-bank.md` when a broader set of prompts is useful.

## Scope Writing Rules

When creating the requested scope document:

- Preserve the section structure from `templates/project-scope-template.md`.
- Replace all instructional placeholder text with project-specific content.
- Use stable requirement IDs such as `FR-001` and `NFR-001`.
- Make functional requirements testable or verifiable.
- Make non-functional requirements measurable or reviewable where practical.
- Separate facts, assumptions, open questions, and recommendations.
- Keep design-level ideas as constraints, assumptions, open questions, or follow-up deliverables unless the user asked for design.
- Include a diagrams section that draws both mandatory project-scope diagrams: C4 System Context and C4 Container.
- Use Mermaid code blocks for diagrams unless the user requests another diagram format.
- Do not create a C4 Component diagram in the project scope document.
- If diagram details are unknown, draw the best accurate high-level diagram and record gaps under `Open Questions`.
- Make scope boundaries explicit enough that future work can be judged as in or out of scope.

## Deep Investigation Standard

Do not rely only on the initial user prompt when the project is still vague.

Investigate the workspace first, then interview the user. For technical projects, identify relevant external systems, upstream APIs, standards, runtime dependencies, deployment environments, security concerns, and test expectations before finalizing scope.

For each important domain claim, prefer one of:

- a workspace source
- an authoritative upstream or official source
- an explicitly labeled assumption
- an explicitly listed open question

## Completion Criteria

The skill is complete when:

- the requested output scope document exists
- every section from `templates/project-scope-template.md` is filled
- requirements have IDs
- C4 System Context and C4 Container diagrams are present as diagram code blocks
- no C4 Component diagram is included
- risks and open questions are captured
- the final response names the file and highlights remaining unresolved items

## Error Handling

If `templates/project-scope-template.md` is missing, inspect nearby `.spec/`, docs, and planning files for an existing scope format. If no usable template exists, ask the user for the intended template or create the requested output scope document with clear standard sections for overview, stakeholders, requirements, non-functional requirements, boundaries, constraints, assumptions, risks, diagrams, deliverables, and open questions.

If an existing scope document is present at the requested output path, preserve confirmed project facts and update only content needed to complete the current scope. Do not discard unresolved questions, assumptions, or decisions unless the user explicitly confirms they are obsolete.

If external research is required but authoritative sources are unavailable, record the affected claim as an assumption or open question. Do not present unverified domain claims as facts.

If the user cannot answer enough questions to complete a section, write the best available project-specific content and capture the gap under `Assumptions` or `Open Questions`.

If workspace context conflicts with user-provided context, prefer the most recent explicit user statement and note the conflict in `Assumptions` or `Open Questions`.
