#include <liblives.hpp>

#include <unistd.h> // for sleep()
#include <iostream> // for cout

int main() {
           using namespace lives;
           using namespace std;

	livesApp *lives = new livesApp;

	sleep(40);

	set cset = lives->getSet();

	cout << "Set name is " << cset.name() << endl;

	sleep(20);

	delete lives;
}
