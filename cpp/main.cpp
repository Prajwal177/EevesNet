#include "main_window.h"

#include <QApplication>

int main(int argc, char* argv[])
{
	QApplication application(argc, argv);
	QApplication::setApplicationName("EevesNet");
	QApplication::setOrganizationName("EevesNet");

	MainWindow window;
	window.showMaximized();

	return application.exec();
}
