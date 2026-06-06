# Desafio 7 - Wokwi + Edge Impulse

Este pacote combina o firmware do Desafio 4 com a biblioteca C++/MCU exportada do Edge Impulse.

Arquivos principais:
- main.cpp: firmware convertido para C++ com chamada a run_classifier().
- CMakeLists.txt: build Pico SDK usando target app.
- diagram.json: circuito Wokwi original.
- edge-impulse/: biblioteca C++/MCU do Edge Impulse.

Observação: no Wokwi web, copiar 1500+ arquivos manualmente é inviável. Use importação por GitHub/VS Code/Wokwi CLI se o editor web não aceitar upload de pasta.
