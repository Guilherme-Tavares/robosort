# Como controlar o motor do EV3 com Python pelo computador via USB

Este metodo roda o Python no computador e envia comandos para o EV3 pela porta USB marcada como `PC`.

Importante: neste modo, o EV3 nao roda o arquivo `.py` dentro dele. O programa roda no computador, e o EV3 recebe os comandos pelo cabo USB.

## Materiais necessarios

- LEGO Mindstorms EV3 Brick.
- Cabo USB conectado na porta do EV3 marcada como `PC`.
- Motor EV3 conectado em uma porta de saida, por padrao `A`.
- Python instalado no computador.
- Biblioteca Python `ev3_dc`.
- Arquivo `pc_usb_motor_control.py`.

## 1. Preparar o EV3

1. Ligue o EV3 normalmente, sem cartao microSD.
2. Aguarde o sistema original LEGO EV3 iniciar.
3. Conecte o motor na porta `A`.
4. Conecte o cabo USB na porta do EV3 marcada como `PC`.
5. Conecte a outra ponta do cabo no computador.

## 2. Instalar a biblioteca no computador

No terminal do computador, execute:

```bash
pip install ev3_dc
```

Se o seu computador usa mais de uma versao do Python, talvez seja necessario usar:

```bash
python -m pip install ev3_dc
```

ou:

```bash
py -m pip install ev3_dc
```

## 3. Conferir a porta do motor no codigo

O arquivo usa a porta `A` por padrao:

```python
PORT = ev3.PORT_A
```

Se o motor estiver conectado em outra porta, altere para uma destas opcoes:

```python
PORT = ev3.PORT_B
PORT = ev3.PORT_C
PORT = ev3.PORT_D
```

Use apenas uma delas.

## 4. Rodar o programa

Na pasta onde esta o arquivo `pc_usb_motor_control.py`, execute:

```bash
python pc_usb_motor_control.py
```

No Windows, tambem pode funcionar com:

```bash
py pc_usb_motor_control.py
```

## 5. Comandos do programa

Depois que o programa conectar ao EV3, use estes comandos no terminal:

| Comando | Funcao |
| --- | --- |
| `l` | Liga ou desliga o motor |
| `+` | Aumenta a velocidade |
| `-` | Diminui a velocidade |
| `d` | Define giro horario |
| `e` | Define giro anti-horario |
| `s` | Mostra o status atual |
| `q` | Para o motor e encerra o programa |

## 6. Valores principais do codigo

No arquivo `pc_usb_motor_control.py`:

```python
speed = 30
direction = 1
is_running = False
```

- `speed`: aceita valores de `1` ate `100`, representando porcentagem da velocidade.
- `direction`: aceita `1` para horario ou `-1` para anti-horario.
- `is_running`: aceita `True` para ligado ou `False` para desligado.

## 7. Observacoes importantes

- O programa depende do computador ficar conectado ao EV3.
- Se o cabo USB for removido, o controle para.
- Esse metodo nao substitui completamente o MicroPython no EV3; ele apenas controla o EV3 remotamente.
- A biblioteca `ev3_dc` usa comandos diretos do EV3 e pode se comunicar por USB com o sistema original.
- Se a conexao falhar, teste outro cabo USB. Alguns cabos servem apenas para carregar energia e nao transmitem dados.

## 8. Erro `usb.core.NoBackendError: No backend available`

Esse erro significa que o Python encontrou o pacote `pyusb`, mas o Windows nao encontrou uma biblioteca USB nativa para ele usar.

Em outras palavras: ainda nao e erro do EV3, do motor ou do cabo. O problema esta na camada USB do computador.

### Opcao recomendada no Windows

1. Instale novamente os pacotes Python:

```bash
py -m pip install --upgrade pyusb ev3_dc
```

2. Instale um backend `libusb` para Windows.

Como o seu Python esta em `Python312`, provavelmente ele e 64 bits. Nesse caso, baixe o pacote oficial do `libusb` para Windows e copie o arquivo:

```text
MinGW64\dll\libusb-1.0.dll
```

para:

```text
C:\Windows\System32
```

Se o seu Python for 32 bits, use o arquivo da pasta `MinGW32\dll` em vez de `MinGW64\dll`.

3. Feche e abra novamente o PowerShell.

4. Teste se o backend USB foi encontrado usando o arquivo auxiliar:

```bash
py check_usb_backend.py
```

Ou, se estiver fora da pasta do projeto:

```bash
py -c "import usb.core; print(usb.core.find())"
```

Se nao aparecer mais `NoBackendError`, o backend foi encontrado. O resultado pode ser `None` caso nenhum dispositivo USB compativel seja detectado naquele momento; isso ainda significa que o backend carregou corretamente.

### Se ainda nao conectar ao EV3

Se o backend foi encontrado, mas o EV3 ainda nao conecta:

1. Confirme que o EV3 esta ligado no sistema original LEGO EV3.
2. Confirme que o cabo esta conectado na porta marcada como `PC`.
3. Teste outro cabo USB.
4. Rode o diagnostico:

```bash
py check_usb_backend.py
```

5. Instale um driver compativel usando o Zadig, selecionando o dispositivo EV3 e o driver `WinUSB` ou `libusbK`.

Tenha cuidado ao usar o Zadig: selecione o dispositivo EV3 correto antes de trocar o driver.

## 9. Erro `[Errno 5] Input/Output Error`

Esse erro geralmente aparece quando o Python ja encontrou o backend USB, mas nao conseguiu conversar corretamente com o EV3.

As causas mais comuns no Windows sao:

- driver USB do EV3 incompativel com acesso via `libusb`;
- cabo USB ruim ou somente de carga;
- EV3 conectado em outra porta que nao seja a porta `PC`;
- EV3 travado ou ainda inicializando.

Tente nesta ordem:

1. Desconecte o cabo USB.
2. Desligue e ligue o EV3.
3. Espere o menu principal do EV3 aparecer.
4. Conecte o cabo na porta `PC`.
5. Rode:

```bash
py check_usb_backend.py
```

6. Se o EV3 aparecer como `VID=0x0694 PID=0x0005`, tente novamente:

```bash
py pc_usb_motor_control.py
```

Se continuar dando `[Errno 5]`, instale um driver usando o Zadig:

1. Baixe o Zadig: https://zadig.akeo.ie/
2. Abra o Zadig como administrador.
3. No menu `Options`, marque `List All Devices`.
4. Selecione o dispositivo LEGO/EV3.
5. Escolha `WinUSB` ou `libusbK`.
6. Clique em `Install Driver` ou `Replace Driver`.
7. Desconecte e conecte o EV3 novamente.
8. Rode o programa outra vez.

Aviso: trocar o driver pode afetar a comunicacao com o software LEGO original no Windows. Se isso acontecer, e possivel voltar o driver pelo Gerenciador de Dispositivos do Windows.

## 10. Referencias

- Documentacao da biblioteca `ev3_dc`: https://ev3-dc.readthedocs.io/
- Exemplos de motor com USB: https://ev3-dc.readthedocs.io/en/latest/examples_motor.html
- FAQ do PyUSB sobre `No backend available`: https://github.com/pyusb/pyusb/blob/master/docs/faq.rst
- Informacoes do libusb para Windows: https://github.com/libusb/libusb/wiki/Windows
