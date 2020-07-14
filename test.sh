#!/usr/bin/env bash
set -o pipefail

RTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

source "${RTDIR}"/test_config.sh

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
    if [[ -n "$NEXUS_AUTH" ]]; then
        echo
        for test_case in "${tests[@]}"; do
            nexus_raw_upload_dir "${test_case}" "${test_data_upload}"
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

setup_deps() {
    if [[ ! -d nexus_bash ]]; then
        git clone https://github.com/jhunkeler/nexus_bash
    fi

    if [[ -n "$test_dep_nexus_bash_rev" ]]; then
        (cd nexus_bash && git checkout $test_dep_nexus_bash_rev) &>/dev/null
    else
        (cd nexus_bash && git pull) &>/dev/null
    fi

    source nexus_bash/nexus_bash.sh
}

# main program
setup_deps
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
