# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository", "git_repository")

_strict = False

def _build_http_archive(
    name,
    remote,
    branch = None,
    commit = None,
    tag = None,
    build_file = None,
    patches = [],
    patch_args = [],
    patch_cmds = [],
    ):
  if not remote.startswith("https://github.com"):
    # This is only currently support for github repos
    return False

  # Fix remote suffix
  if remote.endswith(".git"):
    remote = remote[:-4]
  elif remote.endswith("/"):
    remote = remote[:-1]

  # Tranform repo URL to archive URL
  repo_name = remote.split("/")[-1]
  if tag:
    urls = ["%s/archive/v%s.zip" % (remote, tag)]
    prefix = repo_name + "-" + tag
  if commit or branch:
    ref = commit if commit else branch
    urls = ["%s/archive/%s.zip" % (remote, ref)]
    prefix = repo_name + "-" + ref

  # Generate http_archive rule
  if build_file:
    http_archive(
      name = name,
      urls = urls,
      strip_prefix = prefix,
      build_file = build_file,
      patches = patches,
      patch_args = patch_args,
    )
  else:
    http_archive(
      name = name,
      urls = urls,
      strip_prefix = prefix,
      patches = patches,
      patch_args = patch_args,
    )
  return True

def _build_git_repository(
    name,
    remote,
    branch = None,
    commit = None,
    tag = None,
    build_file = None,
    patches = [],
    patch_args = [],
    patch_cmds = [],
    ):

  # Strip trailing / from remote
  if remote.endswith("/"):
    remote = remote[:-1]

  # Generate the git_repository rule
  if build_file:
    new_git_repository(
      name = name,
      remote = remote,
      branch = branch,
      commit = commit,
      tag = tag,
      patches = patches,
      patch_args = patch_args,
      build_file = build_file,
      patch_cmds = patch_cmds,
    )
  else:
    git_repository(
      name = name,
      remote = remote,
      branch = branch,
      commit = commit,
      tag = tag,
      patches = patches,
      patch_args = patch_args,
      patch_cmds = patch_cmds,
    )
  return True

def remote_workspace(
    name,
    remote,
    branch = None,
    commit = None,
    tag = None,
    build_file = None,
    use_git = False,
    patches = [],
    patch_args = [],
    patch_cmds = [],
    ):
  ref_count = 0
  if branch:
    ref_count += 1
  if commit:
    ref_count += 1
  if tag:
    ref_count += 1

  if ref_count == 0:
    fail("one of branch, commit or tag must be set for " + name)
  elif ref_count > 1:
    fail("only one of branch, commit, or tag can be set for " + name)

  if commit and len(commit) != 40:
    fail("You must use the full commit hash for " + name)
    use_git = True
    print("using git for ", name, remote)

  if _strict and branch:
    print("Warning: external workspace, %s, " % name +
          "should refer to a specific tag or commit (currently: %s)" % branch)

  # Prefer http_archive
  if not use_git and _build_http_archive(
      name, remote, branch, commit, tag, build_file, patches, patch_args, patch_cmds):
    return

  # Fall back to git_repository
  if _build_git_repository(
      name, remote, branch, commit, tag, build_file, patches, patch_args, patch_cmds):
    return

  fail("could not generate remote workspace for " + name)
