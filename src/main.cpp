#include <iostream>

#include <libNTS/nts.hpp>

using std::cout;


int main ( int argc, char **argv )
{
	if ( argc <= 1 )
	{
		cout << "usage: " << argv[0] << " file.ll\n";
		return 0;
	}

	cout << "done\n";
	return 0;
}
