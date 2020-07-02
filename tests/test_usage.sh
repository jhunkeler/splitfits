test_splitfits_usage_longopt() {
    ${test_program} --help
    return $?
}

test_splitfits_usage_shortopt() {
    ${test_program} -h
    return $?
}
