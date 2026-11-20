# App do celular (Android)

Página para ajustar o robô pelo Bluetooth: parâmetros com `+`/`−`, gravar no robô, perfis guardados no celular, barra de sensores ao vivo, resumo e gráfico da última partida, testes (rodas, EDF, mux, IR) e um terminal.

Ela usa o mesmo console de texto do firmware (`firmware/principal/console.cpp`) pelo Bluetooth LE (`canal.cpp`). Não precisa mudar nada no firmware, só gravar a versão com `BLE_CONSOLE 1`.

## Publicar (uma vez)
1. No GitHub: **Settings → General → Danger Zone → Change visibility → Public** (o GitHub Pages grátis exige repositório público).
2. **Settings → Pages → Build and deployment**: Source = *Deploy from a branch*; Branch = o branch do projeto e a pasta `/ (root)` → **Save**.
3. Em 1 ou 2 minutos, a página fica em `https://<usuario>.github.io/Perseguidor-qx/app/`.

## Instalar no celular
1. Abrir o endereço acima no **Chrome** do Android.
2. Menu ⋮ → **Instalar app** (ou "Adicionar à tela inicial").
3. Abrir pelo ícone. Na primeira conexão, o Android pede a permissão de "dispositivos por perto" (ou localização, nas versões antigas).

Depois de instalada, ela abre sem internet. As atualizações chegam sozinhas: quando abrir com internet, ela baixa a versão nova e passa a usá-la na próxima vez.

## Usar
- **Conectar** → escolher "Perseguidor-qx". A senha é a do `BLE_SENHA` (padrão `1234`; muda na aba Terminal → Conexão).
- **Ajustes**: cada mudança vale na hora e fica marcada; o contorno verde quer dizer que o robô confirmou o valor. **Salvar no robô** grava na memória (senão volta ao ligar de novo).
- **Perfis**: guardam os parâmetros atuais no celular com um nome; "Mandar ao robô" envia de volta (depois, Salvar).
- **Sensores**: precisa do robô calibrado (botão 1, ou botão 2 para a calibração gravada).
- **Partida**: depois de uma corrida, "Ler resumo" e "Ler registro" (gráfico dos últimos 2,4 s; "Baixar CSV" para analisar no PC).
- **Testes**: pedem confirmação antes de girar as rodas ou ligar a EDF; o botão **PARAR** interrompe a EDF e os testes contínuos.

O console só responde com o robô parado ("esperando" ou "pronto"); durante a corrida os comandos ficam esperando.

## Problemas comuns
| Sintoma | O que fazer |
|---|---|
| Aviso "Este navegador não tem Web Bluetooth" | Usar o Chrome (não o navegador da Samsung nem o do app do GitHub) e o endereço `https://` |
| O robô não aparece na lista | Bluetooth e localização ligados; robô ligado com o firmware BLE; desconectar outro app (só um celular por vez) |
| "senha recusada" | Conferir a senha na aba Terminal → Conexão |
| Parou de responder | Tocar em Desconectar e depois em Reconectar |

## Arquivos
| Arquivo | O que é |
|---|---|
| `index.html`, `estilo.css`, `app.js` | a página (sem bibliotecas externas) |
| `manifest.webmanifest`, `icones/` | nome, cores e ícones do app instalado |
| `sw.js` | guarda a página no celular. **Troque a `VERSAO` a cada mudança** nos arquivos |

## APK (depois)
Validada a página, o APK pode ser um TWA (Trusted Web Activity, gerado com o Bubblewrap): um app Android que abre esta mesma página pelo Chrome, sem barra de endereço. O WebView comum dos apps não tem Web Bluetooth, por isso o APK não pode ser só "a página dentro de um app".
