# How to Contribute

We'd love to accept your patches and contributions to this project. There are
just a few small guidelines you need to follow.

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution,
this simply gives us permission to use and redistribute your contributions as
part of the project. Head over to <https://cla.developers.google.com/> to see
your current agreements on file or to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

## Submitting Code

### General Information

 - One PR for one feature or change request. Prefer multiple smaller PRs to one gigantic one. Multiple commits during development are OK; see next point.
 
 - Stratum uses a [squash and rebase](https://help.github.com/en/github/collaborating-with-issues-and-pull-requests/about-pull-request-merges#squash-and-merge-your-pull-request-commits) model. You do **not** have to do this by hand! GitHub will guide you through it, if possible.
 
 - Describe what your PR solves and why. Consider opening a separate issue describing the technical details there and [link it to the PR](https://help.github.com/en/github/managing-your-work-on-github/closing-issues-using-keywords). This keeps code review and design discussions clean.

### Steps to Follow

1. Make sure your code passes unit tests. Create new ones for new code and **add them to the [build-targets.txt](.circleci/build-targets.txt) and [test-targets.txt](.circleci/test-targets.txt) files** in the same PR, so CI can pick them up! You can also run them locally with bazel: `xargs -a .circleci/test-targets.txt bazel test`

2. Check code style with `cpplint` and `clang-format` (pre-installed in dev docker image).

3. Create a [PR on Github](https://github.com/stratum/stratum/compare). Either directly on the stratum repo or in your own fork. (Consider [allowing maintainers](https://help.github.com/en/github/collaborating-with-issues-and-pull-requests/allowing-changes-to-a-pull-request-branch-created-from-a-fork) to make changes)

4. Wait for [CI checks](https://circleci.com/gh/stratum/stratum) to pass. You can check the [coverage report](https://codecov.io/gh/stratum/stratum) after they ran. Repeat steps 1. and 2. as necessary. **Passing CI is mandatory.**

5. Await review. Everyone can comment on code changes, but only Collaborators and above can give final review approval. **All changes must get at least one approval**. Join one of the [communication channels](https://wiki.opennetworking.org/display/COM/Stratum+Wiki+Home+Page) to request a review or to bring additional attention to your PR.

## Community Guidelines

This project follows [Google's Open Source Community
Guidelines](https://opensource.google.com/conduct/) and ONF's [Code of Conduct](https://github.com/stratum/stratum/blob/master/CODE_OF_CONDUCT.md).
