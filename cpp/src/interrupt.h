volatile bool interrupt_received = false;
void InterruptHandler(int signo) {
  interrupt_received = true;
}

char getch();