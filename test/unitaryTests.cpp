// test/unitaryTests.cpp
#include "process_runner.h"
#include <cassert>
#include <iostream>
#include <sstream>

// Função para simular o envio de uma mensagem
void simulate_client_server_routine(const std::string& node_id) {
  try {
    run_process(node_id);
    std::cout << "Processo completo executado sem erros!" << std::endl;
  } catch (const std::exception& e) {
    assert(false); // O teste falha se lançar exceção
  }
}

void test_complete_routine() {
  simulate_client_server_routine("1"); // todo: arrumar
}

// int main() {
//   test_complete_routine();
//   std::cout << "Todos os testes passaram!" << std::endl;
//   return 0;
// }
