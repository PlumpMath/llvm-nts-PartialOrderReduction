add_executable ( run
	"main.cpp"
)
include_directories ( ${LLVM_INCLUDE_DIRS} )

target_link_libraries ( run "nts-seq" "llvm2nts" )

