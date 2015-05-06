add_executable ( po
	"control_flow_graph.cpp"
	"main.cpp"
	"tasks.cpp"
	"logic_utils.cpp"
)

include_directories ( ${LLVM_INCLUDE_DIRS} )
target_link_libraries ( po "llvm2nts" "NTS_cpp" )

