# ZoloOS — Project Guidelines

## Code Comments

- Include real-world simple analogies to explain low-level hardware/OS concepts
  (e.g. GDT = badge system, IDT = emergency contact list)
- Write comments so a software engineer unfamiliar with OS/low-level development
  can follow along — explain the "why", not just the "what", and define any
  hardware-specific terms before using them

## Commit Style

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<optional scope>): <description>
```

Common types: `feat`, `fix`, `chore`, `refactor`, `docs`, `test`

Examples:
- `feat: add VGA text mode driver`
- `feat(boot): add multiboot header and kernel entry point`
- `fix(vga): correct scrolling when buffer overflows`
- `chore: add .gitignore`
