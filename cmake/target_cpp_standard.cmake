function(set_target_cxx_standard_private target std)
    target_compile_features(${target}
        PRIVATE "cxx_std_${std}"
    )
endfunction()

function(set_target_cxx_standard_interface target std)
    target_compile_features(${target}
        INTERFACE "cxx_std_${std}"
    )
endfunction()

function(set_target_cxx_standard_public target std)
    target_compile_features(${target}
        PUBLIC "cxx_std_${std}"
    )
endfunction()