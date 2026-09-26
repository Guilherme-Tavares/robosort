# Regras para alterações de código

Estas regras se aplicam especificamente a tarefas que escrevam ou alterem código neste repositório.

## Contexto antes de alterar

- Antes de começar, leia todos os arquivos Markdown (`*.md`) do repositório para entender o sistema.
- Ao trabalhar em um módulo, leia também todo o código necessário desse módulo e suas integrações para compreender o impacto da mudança.
- Ao delegar trabalho a outros agentes, transmita e exija o cumprimento destas regras.

## Git e validação

- Nunca inclua referências a IA em commits. Não use assinaturas ou marcações como `Co-authored-by: Claude`, `made by Codex`, `gerado por IA` ou equivalentes.
- Só execute `git commit` ou `git push` após autorização explícita do usuário.
- Antes de qualquer commit autorizado, execute os testes necessários e confirme que estão passando. Se algum teste falhar, informe o usuário antes de prosseguir.
- Enquanto não houver suíte de testes configurada, registre essa limitação; a exigência de executar a suíte passa a valer assim que ela existir.

## Qualidade e nomenclatura

- Escreva código limpo e siga as convenções, a formatação, os idiomas e as boas práticas oficiais da linguagem e do framework em uso.
- Preserve em inglês os nomes exigidos ou universalmente convencionais da linguagem, do framework ou de APIs, como `get`, `set`, hooks e métodos de ciclo de vida.
- Dê nomes em português do Brasil às funções de lógica própria do projeto, como `gerenciarClima()` em vez de `manageWeather()`.

## Segurança

- Nunca exponha credenciais, chaves de API, senhas ou tokens no código. Use variáveis de ambiente.
- Não inclua no versionamento `.env`, chaves privadas, certificados ou outros arquivos com dados confidenciais.
- Confira o `.gitignore` antes de preparar um commit. Antes do primeiro commit de qualquer subprojeto sem `.gitignore`, crie um arquivo adequado para ele.
- Não execute ações destrutivas, incluindo apagar arquivos ou pastas, remover dados ou bancos e reescrever o histórico do Git, sem confirmação explícita do usuário.
- Não envie código nem dados do projeto para serviços externos sem autorização.
- Se houver dúvida relevante sobre o risco de uma ação, pare e peça orientação ao usuário.
