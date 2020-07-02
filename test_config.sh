test_program="${RTDIR}/splitfits"
test_program_version="$(git describe --always --tags --long)"
test_data="${RTDIR}/data"
test_data_remote=https://nx.astroconda.org/repository
test_data_remote_auth=${test_data_remote_auth:-}
test_data_upload="generic/spb-splitfits"
if [[ $CIRCLECI == "true" ]]; then
    test_data_upload="${test_data_upload}/ci/${CIRCLE_BRANCH}_${CIRCLE_JOB}/${CIRCLE_BUILD_NUM}/${test_program_version}"
else
    test_data_upload="${test_data_upload}/user/${test_program_version}"
fi
