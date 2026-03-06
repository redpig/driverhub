# CLAUDE.md — AI Assistant Guide for DriverHub

## Project Overview

DriverHub is a new project. This file establishes conventions and guidelines for AI assistants working in this repository.

## Repository Structure

```
driverhub/
├── CLAUDE.md          # AI assistant guidelines (this file)
└── .git/              # Git configuration
```

> **Note:** This repository is in its initial state. Update this section as the project structure evolves.

## Development Workflow

### Branch Conventions

- Feature branches: `feature/<description>`
- Bug fix branches: `fix/<description>`
- Documentation branches: `docs/<description>`
- Always branch from `main`

### Commit Messages

Follow conventional commit format:

```
<type>(<scope>): <short summary>

<optional body>
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `ci`

Examples:
- `feat(api): add driver registration endpoint`
- `fix(auth): resolve token refresh race condition`
- `docs: update CLAUDE.md with build instructions`

### Pull Requests

- Keep PRs focused on a single concern
- Include a clear description of what changed and why
- Reference related issues when applicable

## Build & Test Commands

> **TODO:** Update this section once the tech stack and build system are established.

```shell
# Placeholder — replace with actual commands
# npm install / pip install -r requirements.txt / cargo build
# npm test / pytest / cargo test
# npm run lint / ruff check . / cargo clippy
```

## Code Style & Conventions

### General Principles

- Write clear, readable code over clever code
- Keep functions focused and small
- Use descriptive names for variables, functions, and types
- Add comments only where the intent isn't obvious from the code itself
- Don't over-engineer — solve the problem at hand

### File Organization

- Group related files together in directories
- Keep a flat structure where possible; avoid deep nesting
- Name files descriptively — prefer `user_registration.py` over `utils2.py`

## Key Architectural Decisions

> **TODO:** Document architectural decisions as they are made (e.g., database choice, API style, authentication approach).

## Environment & Configuration

- Do not commit secrets, credentials, or `.env` files
- Use environment variables for configuration that varies between environments
- Document required environment variables in a `.env.example` file

## Guidelines for AI Assistants

1. **Read before writing** — Always read existing code before modifying it
2. **Minimal changes** — Only change what's needed to accomplish the task
3. **No speculative features** — Don't add functionality that wasn't requested
4. **Run tests** — After making changes, run the test suite if one exists
5. **Check for lint** — Run linters/formatters before committing
6. **Commit atomically** — Each commit should represent one logical change
7. **Don't commit secrets** — Never commit `.env`, credentials, API keys, or tokens
