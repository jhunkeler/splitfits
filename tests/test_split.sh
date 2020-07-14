test_splitfits_data1() {
    local datafile

    datafile=$(nexus_raw_download repository/generic/fits/sample.fits)
    [[ ! -f ${datafile} ]] && return 1

    if ! ${test_program} -o ${test_case} ${datafile}; then
        return 1
    fi

    find . -regex '.*\.part_[0-9]+' | xargs -I'{}' cat '{}' >> streamed.fits
    if ! diff ${datafile} streamed.fits; then
        return 1
    fi

    return 0
}

test_splitfits_combine_data1() {
    local datafile

    datafile=$(nexus_raw_download repository/generic/fits/sample.fits)
    [[ ! -f ${datafile} ]] && return 1

    if ! ${test_program} -o ${test_case} ${datafile}; then
        return 1
    fi

    if ! ${test_program} -c ${test_case}/$(basename ${datafile}).part_map; then
        return 1
    fi

    return 0
}
