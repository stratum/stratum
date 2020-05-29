# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

def stratum_package(*args, **kwargs):
    #FIXME
    pass

# This rules works exactly like pkg_tar from @rules_pkg, except that it preserves
# symlinks in the resulting tarball.
# This is a workaround for https://github.com/bazelbuild/rules_pkg/issues/115
# TODO(unknown): Remove once fixed upstream.
def pkg_tar_with_symlinks(
        name,
        srcs,
        package_dir = "",
        strip_prefix = "",
        mode = "0644"):
    cmd = '''
    echo "--owner=0.0" >> "$@"
    echo "--owner_name=." >> "$@"
    echo "--mtime=portable" >> "$@"
    '''
    cmd += 'echo "--directory=%s" >> "$@"\n' % package_dir
    cmd += 'echo "--mode=%s " >> "$@"' % mode
    flag_file = "%s-tar-flags" % name
    native.genrule(
        name = flag_file,
        srcs = srcs,
        outs = [name + "-tar.flags"],
        cmd = cmd + """
            dir=%s
            for f in $(SRCS); do
                symlink_target=$$(readlink $$f | xargs -I{} find {} -type l | xargs readlink || true)
                if [[ -n $$symlink_target ]]; then
                    echo "--link=$${dir#*/}/$${f#*%s/}:$$symlink_target" >> "$@"
                else
                    echo "--file=$$f=$${f#*%s}" >> "$@"
                fi
            done""" % (package_dir, strip_prefix, strip_prefix),
    )
    native.genrule(
        name = name,
        srcs = srcs + [":" + flag_file],
        outs = [name + ".tar"],
        cmd = "./$(location @bazel_tools//tools/build_defs/pkg:build_tar) --output $@ --flagfile $(location :%s)" % flag_file,
        tools = ["@bazel_tools//tools/build_defs/pkg:build_tar"],
    )
