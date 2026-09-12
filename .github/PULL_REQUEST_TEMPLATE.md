<!--
Thanks for contributing to Veil.

Keep the title short and imperative, e.g. "Skip the alpha channel on RGBA
carriers". Mark the pull request as a draft while it is still in progress.
-->

## Summary

<!-- What does this change do, and why? One or two paragraphs. -->

## Related issues

<!-- e.g. "Closes #123", "Part of #45". Write "None" if this isn't tracked. -->

## Type of change

<!-- Check every box that applies, and add the matching type/* label. -->

- [ ] Bug fix (`type/bug`)
- [ ] New feature (`type/feature`)
- [ ] Enhancement to existing behavior (`type/enhancement`)
- [ ] Refactor, no behavior change (`type/refactor`)
- [ ] Performance (`type/performance`)
- [ ] Documentation (`type/documentation`)
- [ ] Chore / maintenance (`type/chore`)

## Container compatibility

<!--
Does this change what goes into the image, or how it is read back? Anything
that touches the preamble, the sealed header, the marker, the slot mapping or
the scatter walk breaks containers written by older builds, and needs
LSB_VERSION bumped. Otherwise write "None".
-->

None

## How this was tested

<!--
Which tests did you add, and how did you verify the change by hand? Include the
commands you ran. A round trip through a real image is the usual proof:
-->

```
$ make test
$ ./veil encode carrier.png payload.bin -o out.png -c lsbm
$ ./veil decode out.png -o recovered.bin
$ ./devtools/checksum.sh payload.bin recovered.bin
```

- Compiler tested: <!-- e.g. gcc 14, clang 19 -->

## Checklist

- [ ] `make test` passes
- [ ] `CHANGELOG.md` has an entry under `## [Unreleased]`, or this pull
      request carries the `ci/skip-changelog` label
- [ ] Documentation (README, comments) is updated where it applies
- [ ] No secret material is left in a buffer that isn't wiped with
      `sodium_memzero` on every path out, error paths included
