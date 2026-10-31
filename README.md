# OptiBob — plugin audio (AU / VST3)

Emulazione originale del processing broadcast in stile Optimod, scritta da zero
misurando materiale reale. Non contiene codice Orban.

La build (VST3 Windows, AU+VST3 macOS) parte da sola su GitHub Actions a ogni
push. Gli artefatti si scaricano dalla scheda **Actions** del repository.

Il motore è lo stesso di OptiBob Live; gli header in `Engine/` si aggiornano con
`sincronizza.command`.
