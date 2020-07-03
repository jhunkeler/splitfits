test_splitfits_usage_longopt_help() {
    ${test_program} --help
    return $?
}

test_splitfits_usage_shortopt_help() {
    ${test_program} -h
    return $?
}

test_splitfits_usage_longopt_version() {
    result=$(${test_program} --version)
    retval=$?
    
    if [[ -z $result ]]; then
        return 1
    fi
    return $retval
}

test_splitfits_usage_shortopt_version() {
    result=$(${test_program} -V)
    retval=$?
    
    if [[ -z $result ]]; then
        return 1
    fi
    return $retval
}
