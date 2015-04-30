add_executable ( po
	"main.cpp"
)

target_link_libraries ( po "llvm2nts" "NTS_cpp" )
