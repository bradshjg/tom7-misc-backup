int _putc(int);

int main(int argc, char **argv) {
  argc--;
  _putc(argc + (int)'o');
  _putc((int)'k' - argc);
  _putc((int)'!' ^ argc);
  return 0;
}
