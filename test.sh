#!/usr/bin/env bash
set -o pipefail

RTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

source "${RTDIR}"/test_config.sh

get_data_exists() {
    local url
    local response

    url="${test_data_remote}/$1"
    if [[ -z $url ]]; then
        echo "get_data_exists: requires a path relative to $test_data_remote"
        exit 1
    fi

    response=$(curl -s --head "$url" | head -n 1 | awk '{print $2}')
    if (( $response != 200 )); then
        return 1;
    fi
    return 0;
}

get_data() {
    local url
    local filename

    if [[ ! -d "${test_data}" ]]; then
        mkdir -p "${test_data}"
    fi

    url="${test_data_remote}/$1"
    if [[ -z $url ]]; then
        echo "get_data: requires a path relative to $test_data_remote"
        exit 1
    fi

    filename=$(basename $url)
    if ! get_data_exists "$1"; then
        echo "${url}: not found on remote server" >&2
        return 1
    fi

    if ! (cd $test_data && curl -L -O "$url"); then
        echo "Could not download data" >&2
        return 1
    fi

    echo "$test_data/$filename";
}

put_data() {
    local url
    local filename

    url="${test_data_remote}/$2"
    filename="$1"

    if [[ -z $url ]]; then
        echo "put_data: requires a path relative to $test_data_remote"
        return 1
    fi

    if ! curl -s --user "${test_data_remote_auth}" --upload-file "${filename}" "${url}/${filename}"; then
        echo "Could not upload '${filename}' to '${url}'" >&2
        return 1
    fi

    return 0
}

put_result() {
    local src;
    local dest;
    src="$1"
    dest="$2"

    if [[ ! -d "$src" ]]; then
        echo "push_result: source directory does not exist: ${src}" >&2
        return 1
    fi

    if [[ -z "$dest" ]]; then
        echo "push_result: requires a destination relative to ${test_data_remote}" >&2
        return 1
    fi

    for f in $(find "${src}" -type f); do
        echo "Uploading '$f' to '$dest'"
        if ! put_data "$f" "${dest}"; then
            echo "Failed uploading '$src' to '$dest'" >&2
            return 1
        fi
    done;
    return 0
}

read_tests() {
    for f in "${RTDIR}"/tests/test_*; do
        source "$f" || return 0
    done
    return 1
}

export_tests() {
    tests=(
        $(declare -f | cut -d ' ' -f 1 | grep -E '^test_')
    )
    total_passed=0
    total_failed=0
    total_tests=${#tests[@]}
}

run_tests() {
    for (( i=0; i < ${total_tests}; i++ )); do
        just_failed=0
        test_case="${tests[$i]}"
        rm -rf "${test_case}"
        mkdir -p "${test_case}"
        pushd "${test_case}" &>/dev/null
            log_file="output_${test_case}.log"

            /bin/echo -n -e "[$i] ${test_case}... "
            "$test_case" 2>&1 | tee ${log_file} &>/dev/null
            retval=$?

            if [[ "$retval" -ne "0" ]]; then
                echo "FAILED ($retval)"
                (( just_failed++ ))
                (( total_failed++ ))
            else
                echo "PASSED"
                (( total_passed++ ))
            fi

            if [[ ${just_failed} -ne 0 ]] && [[ -s ${log_file} ]]; then
                echo "# OUTPUT:"
                cat "${log_file}"
            fi
        popd &>/dev/null
    done

    # Upload all artifacts in all test directories
    if [[ -n "$test_data_remote_auth" ]]; then
        echo
        for (( i=0; i < ${total_tests}; i++ )); do
            test_case="${tests[i]}"
            put_result ${test_case} ${test_data_upload}
        done
    fi
}

check_runtime() {
    if [[ ! -f "${test_program}" ]]; then
        echo "Tests cannot run without: ${test_program}" >&2
        exit 1
    fi
}

show_stats() {
    printf "\n%d test(s): %d passed, %d failed\n" ${total_tests} ${total_passed} ${total_failed}
}

# main program
check_runtime

if read_tests; then
    echo "Failed to aggregate tests." >&2
    exit 1
fi

export_tests
run_tests
show_stats

if (( total_failed )); then
    exit 1
fi

exit 0
