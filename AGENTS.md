# AI Development Rules

Before any analysis, code change, build, debug, or Flash operation, every AI
working in this repository must read `docs/AI_HANDOFF.md` in full.

After each development task, the AI must update `docs/AI_HANDOFF.md` with the
current implementation state, validation evidence, changed hardware or tool
facts, and any remaining risks before handing the task back.

Do not commit generated build output, `.venv`, CMSIS-Pack downloads, or tool
logs. Follow the Flash-write boundary documented in the handoff file.

Every Git commit must use a detailed message: a concise subject followed by a
body that describes the changed behavior, affected tools or hardware, and
verification performed. Do not use a one-line-only commit message.
