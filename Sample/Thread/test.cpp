#include "Concurrency/Thread.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
	using std::cout, std::boolalpha, std::endl;

	Eterfree::Thread thread;
	cout << thread.getID() << endl;

	thread.configure([] \
	{ cout << "Eterfree" << endl; }, nullptr);
	cout << boolalpha << thread.notify() << endl;

	thread.destroy();

	thread.create();
	cout << thread.getID() << endl;

	thread.configure([] \
	{ cout << "Solifree" << endl; }, nullptr);
	cout << boolalpha << thread.notify() << endl;
	return EXIT_SUCCESS;
}
