"""
Test leak_check_nodejs.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import http, build_config, build_pkg, fs_utils


def http_request():
    return http.post(
        "http://127.0.0.1:8002/",
        {
            "ten": {
                "name": "test",
            },
        },
    )


def test_leak_check_nodejs():
    """Test client and app server."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    app_dir_name = "leak_check_nodejs_app"
    app_root_path = os.path.join(base_path, app_dir_name)
    app_language = "nodejs"

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

    if build_config_args.ten_enable_integration_tests_prebuilt is False:
        # Before starting, cleanup the old app package.
        fs_utils.remove_tree(app_root_path)

        print(f'Assembling and building package "{app_dir_name}".')

        rc = build_pkg.prepare_and_build_app(
            build_config_args,
            root_dir,
            base_path,
            app_dir_name,
            app_language,
        )
        if rc != 0:
            assert False, "Failed to build package."

        # Compile typescript extensions.
        rc = build_pkg.build_nodejs_extensions(app_root_path)
        if rc != 0:
            assert False, "Failed to build TypeScript extensions."

    if sys.platform == "linux":
        if build_config_args.enable_sanitizer:
            libasan_path = os.path.join(
                base_path,
                (
                    "leak_check_nodejs_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

    # If TEN_ENABLE_INTENTIONAL_MEMORY_LEAK is set, the memory leak will be
    # triggered, and at the same time, TEN_ENABLE_MEMORY_TRACKING is set to
    # true, so the memory leak will be detected no matter whether asan is
    # enabled.
    my_env["TEN_ENABLE_MEMORY_TRACKING"] = "true"
    my_env["TEN_ENABLE_INTENTIONAL_MEMORY_LEAK"] = "true"

    server_cmd = os.path.join(base_path, "leak_check_nodejs_app/bin/start")

    if not os.path.isfile(server_cmd):
        print(f"Server command '{server_cmd}' does not exist.")
        assert False

    server = subprocess.Popen(
        server_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=app_root_path,
    )

    is_started = http.is_app_started("127.0.0.1", 8002, 30)
    if not is_started:
        print("The leak_check_nodejs is not started after 30 seconds.")

        server.kill()
        exit_code = server.wait()
        print("The exit code of leak_check_nodejs: ", exit_code)

        assert exit_code == 0
        assert False

    try:
        print("Sending HTTP request to the app server.")
        resp = http_request()
        assert resp != 500
        print(resp)
    finally:
        is_stopped = http.stop_app("127.0.0.1", 8002, 30)
        if not is_stopped:
            print("The leak_check_nodejs can not stop after 30 seconds.")
            server.kill()

        exit_code = server.wait()
        print("The exit code of leak_check_nodejs: ", exit_code)

        # The exit code 123 is used to indicate that the memory leak is
        # detected. This is a magic number that is not used by any other system.
        assert exit_code == 123

        if build_config_args.ten_enable_tests_cleanup is True:
            # Testing complete. If builds are only created during the testing
            # phase, we can clear the build results to save disk space.
            fs_utils.remove_tree(app_root_path)
