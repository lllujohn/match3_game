---
# Pull Request template
## Summary
<!-- Describe what this PR changes and why. -->

## Type of Change
- [ ] `feat:` — New feature (non-breaking)
- [ ] `fix:` — Bug fix
- [ ] `refactor:` — Code refactor (no functional change)
- [ ] `perf:` — Performance improvement
- [ ] `test:` — Adding or fixing tests
- [ ] `docs:` — Documentation only
- [ ] `chore:` — Build, CI, dependency updates
- [ ] `style:` — Code style / formatting

## Testing
- [ ] All existing unit tests pass (`ctest`)
- [ ] New unit tests added (if applicable)
- [ ] Tested on macOS
- [ ] Tested on Linux
- [ ] Tested on Windows (optional)
- [ ] ASan / Valgrind clean

## Checklist
- [ ] Code follows C11 strict standard (no C++ features)
- [ ] No memory leaks (Valgrind or ASan verified)
- [ ] Commit messages follow Conventional Commits
- [ ] Doxygen comments added/updated for any new public APIs
- [ ] `clang-format` applied (`clang-format -i src/*.c include/*.h`)
