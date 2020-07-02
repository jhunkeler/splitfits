test_program="${RTDIR}/splitfits"
test_program_version="$(git describe --always --tags --long)"
test_data="${RTDIR}/data"
test_data_remote=https://nx.astroconda.org/repository
test_data_remote_auth=${test_data_remote_auth:-}
test_data_upload="generic/spb-splitfits/${test_program_version}"
