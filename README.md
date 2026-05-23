# Processamento-Gr-fico-Vivencial-2
Tarefa realizada na semana presencial da disciplina de Processamento Gráfico, onde aprofundamos os conhecimentos em: mapeamento de texturas e transformações nos objetos

Nomes : Enzo Gabriel Franco Lanius e Felipe Plentz Klein

Parallax Scrolling — Processamento Gráfico
Descrição
Extensão do exercício de mapeamento de texturas e transformações, implementando um cenário com efeito parallax e movimentação de personagem.
Funcionalidades

Personagem controlado pelas setas do teclado (ou WASD), movendo-se nos eixos X e Y
Cenário em camadas sobrepostas, do fundo ao primeiro plano
Efeito parallax: camadas próximas se deslocam mais rápido que as distantes
Projeção ortográfica paralela: coordenadas em pixels (800×600), 1 unidade = 1 pixel
Transparência: suporte a PNG com canal alpha para o personagem e camadas

Controles
TeclaAção← / AMover personagem para a esquerda→ / DMover personagem para a direita↑ / WMover personagem para cima↓ / SMover personagem para baixoESCFechar a aplicação

Estrutura de Arquivos
projeto/
├── main.cpp
├── glad/
├── GLFW/
    
Fatores de Parallax
Cada camada possui um fator de deslocamento proporcional à sua profundidade:
Camada Fator X Descrição 
Céu 0.05 Quase estático; Nuvens 0.10 Movimento muito lento; Montanhas 0.20 Movimento lento; Árvores distantes 0.40 Movimento médio; Árvores próximas 0.70 Movimento rápido; Chão 1.00 Acompanha o personagem.

Dependências:

GLAD
GLFW
stb_image.h
OpenGL
