add_library ( nts-seq SHARED
	"control_flow_graph.cpp"
	"nts-seq.cpp"
	"tasks.cpp"
	"logic_utils.cpp"
)

target_link_libraries ( nts-seq "NTS_cpp" )

