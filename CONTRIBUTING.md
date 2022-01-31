<!--
Copyright 2019-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->
# How to Contribute

We'd love to accept your patches and contributions to this project. There are
just a few small guidelines you need to follow.

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution,
this simply gives us permission to use and redistribute your contributions as
part of the project. Head over to <https://cla.opennetworking.org/> to see
your current agreements on file or to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

## Guides, Rules and Best Practices

Stratum follows [Google's Engineering Practices](https://google.github.io/eng-practices/),
[C++ Style Guide](https://google.github.io/styleguide/cppguide.html) and
[unit test rules](stratum/docs/testing.md). Use these documents as a guide when
writing, submitting or reviewing code. Also see Abseil's [Tip of the Week](abseil.io/tips/)
series with in-depth information about select C++ topics.

### Submitting Code

Some additional points for developers:

 - Submit your changes early and often. GitHub has
   [Draft PRs](https://github.blog/2019-02-14-introducing-draft-pull-requests/)
   that allow you to share your code with others during development. Input and
   corrections early in the process prevent huge changes later.

 - Stratum uses a [squash and rebase](https://help.github.com/en/github/collaborating-with-issues-and-pull-requests/about-pull-request-merges#squash-and-merge-your-pull-request-commits)
   model. You do **not** have to do this by hand! GitHub will guide you through
   it, if possible.

 - Consider opening a separate issue describing the technical details there and
   [link it to the PR](https://help.github.com/en/github/managing-your-work-on-github/closing-issues-using-keywords).
   This keeps code review and design discussions clean.

#### Steps to successful PRs

1. Fork Stratum into your personal or organization account via the fork button
   on GitHub.

2. Make your code changes.

3. Pass all unit tests locally. Create new tests for new code and **add the
   targets to the [build-targets.txt](.circleci/build-targets.txt) and
   [test-targets.txt](.circleci/test-targets.txt) files** in the same PR, so CI
   can pick them up! Execute the following command in the Stratum root directory
   to run all currently enabled tests:
   `xargs -a .circleci/test-targets.txt bazel test`

4. Check code style compliance with the `check-*.sh` scripts in the `.circleci`
   directory.

5. Create a [Pull Request](https://github.com/stratum/stratum/compare). Consider
   [allowing maintainers](https://help.github.com/en/github/collaborating-with-issues-and-pull-requests/allowing-changes-to-a-pull-request-branch-created-from-a-fork)
   to make changes if you want direct assistance from maintainers.

6. Wait for [CI checks](https://circleci.com/gh/stratum/stratum) to pass. You
   can check the [coverage report](https://codecov.io/gh/stratum/stratum) after
   they ran. Repeat steps 3. and 4. as necessary. **Passing CI is mandatory.**
   If the CI check does not run automatically, make sure you [unfollow your fork](https://support.circleci.com/hc/en-us/articles/360008097173)
   on CircleCI.

7. Await review. Everyone can comment on code changes, but only Collaborators
   and above can give final review approval. **All changes must get at least one
   approval**. Join one of the [communication channels](https://wiki.opennetworking.org/display/COM/Stratum+Wiki+Home+Page)
   to request a review or to bring additional attention to your PR.

## Core Contributors
Anyone with a Github account can open new issues, comment on existing issues, or
contribute code by opening a pull request from a fork.

A **“core contributor”** is someone who can manage Github issues, approve and
merge pull requests, and create new branches in the main repository.

Core contributors are responsible for maintaining the quality of contributions
to the codebase. The goal of this program is to have a diverse group of
individuals whose expertise in aggregate covers the entire project.

The benefits of being a core contributor include:
- Increased influence of the direction of the project,
- The ability to create branches in the main repository and merge your own code,
  and
- Community recognition and visibility for their contributions and expertise.

### Becoming a Core Contributor

Core contributor candidates need to have a demonstrated proficiency with the
Stratum codebase and a track record of code reviews.  Members of the Technical
Steering Team (TST) and existing core contributors will regularly invite people
to become new core contributors. Nominations can also be made (including
self-nominations) to the TST at any time.

A good nomination will include details about who the person is (including their
Github username) and outline their experience with the Stratum codebase.
Nominations are intended to start a conversation that results in a decision to
make the person a core contributor – anyone whose nomination is not initially
approved is encouraged to gain more experience with code submission and code
review in order to gain further mastery over the codebase. Partial approval is
also possible (e.g. a person may be granted the ability to triage issues, but
not merge pull requests), and full approval may be granted after the contributor
has gained more experience.

New core contributors will be assigned a mentor that is either a TST member or
existing core contributor. The mentor will serve as the primary point of contact
to help onboard the new core contributors and answer any questions they have
with their new responsibilities. The mentor is not the only point of contact,
and core contributors should feel free to reach out to others if and when they
have questions or concerns.

### Tips for Core Contributors
For your own contributions, you now have the ability to approve and merge your
own code. For larger or potentially controversial pull requests, please give the
community an opportunity (at least a few business days) to review your
contribution. **With great power comes great responsibility; please don't abuse
this privilege.**

For branches in the main repository, please try to use sensible names (which
could include your username, issue number, and/or a short descriptive tag).

Stratum follows [Google’s best practices for code review](https://google.github.io/eng-practices/review/reviewer/).
You should follow these guidelines when reviewing pull requests.

If you are unsure about something in an issue or pull request, leave a comment
that outlines your concerns. If a resolution is difficult to reach in the
comments section, the TST meetings are a good place to raise your concerns and
have a discussion.

## Community Guidelines

This project follows [Google's Open Source Community Guidelines](https://opensource.google.com/conduct/)
and ONF's [Code of Conduct](CODE_OF_CONDUCT.md).
