#include "app/Application.hpp"

int main(int argc, char** argv) {
    app::Application application(app::Options::Parse(argc, argv));
    application.Run();
    return 0;
}
