NEXUS_URL=https://nx.astroconda.org
NEXUS_AUTH=${NEXUS_AUTH:-}
test_program="${RTDIR}/splitfits"
test_program_version="$(git describe --always --tags --long)"
test_data="${RTDIR}/data"
test_data_upload="repository/generic/spb-splitfits"
test_dep_nexus_bash_rev=a2586e952bb9e5e70a2d62c55337574d7086b9a0
if [[ $CIRCLECI == "true" ]]; then
    test_data_upload="${test_data_upload}/ci/${CIRCLE_BRANCH}_${CIRCLE_JOB}/${CIRCLE_BUILD_NUM}/${test_program_version}"
else
    test_data_upload="${test_data_upload}/user/${test_program_version}"
fi
