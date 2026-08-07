#if defined(SQUIFLOW_WITH_QT)
int squiflow_workstation_main_qt(int argc, char** argv);
int main(int argc, char** argv) {
    return squiflow_workstation_main_qt(argc, argv);
}
#else
int main() { return 0; }
#endif
