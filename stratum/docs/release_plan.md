# Stratum Release Plan

Stratum follows the "live at head" philosophy where all development happens at HEAD.
This means there are no long term support (LTS) branches, no feature backports and
we do not practice semantic versioning (SemVer). In return, this lightweight approach
allows us to be fast moving and adaptive.

Users are exptected to regularly update to the latest version, but for deployments we
offer quarterly releases. These are tagged off the master branch at the time of
creation and left untouchted after that. A release contains a list of changes, new
platforms and pre-built artifacts. We name them based on the release date in the form
`year.month`, e.g., `21.03` for the March 2021 release. All releases can be found on
the GitHub [release](https://github.com/stratum/stratum/releases) page.

## Breaking Changes

> How are breaking changes handled without SemVer?

For breaking changes we do non-atomic refactoring with continuous deployments, 
as seen in other projects like Abseil, Bazel or Protobuf:

0. Release N: Contains old feature
1. New feature added at head
    - Discuss at TST meetings and on pull requests
    - Ensure no changes to the old feature (sometimes this means introducing a new target)
    - Include tools and docs for assisted migration
    - Mark old functionality as deprecated
2. Release N+1: New feature is included
    - Old and new feature is available; old one is deprecated (expect logs)
3. Release N+2: Old feature is removed
    - Clients must use the new feature
