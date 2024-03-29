﻿#include "Thread.h"

#include <cstdlib>
#include <iostream>

int main()
{
	using std::cout, std::boolalpha, std::endl;

	ETERFREE_SPACE::Thread thread;
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
