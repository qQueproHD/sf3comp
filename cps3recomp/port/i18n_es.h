// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); from cps2recomp.
//
// The port's own words in this language (i18n.h); cps2recomp's translations of the same words.
#pragma once

#include "../recomp-common/i18n.h"

namespace port {

inline constexpr recomp::i18n::Entry kSpanish[] = {
    {"Video", "Vídeo"},
    {"What you play and hear, recorded as an MP4 video that plays anywhere.", "Lo que juegas y oyes, grabado como un vídeo MP4 que se reproduce en cualquier parte."},
    {"Record video", "Grabar vídeo"},
    {"Records the game as you play it, with its sound (the custom soundtrack's too), as an MP4 video in videos beside your saves, until this row again (Stop recording) or Alt+F12. The picture is as screenshots take it, readouts and all, without the lines at the bottom; the video's time is the game's: stopped or in this menu nothing is recorded, and slowed or sped up it plays at the game's own speed.", "Graba el juego tal como lo juegas, con su sonido (también el de la banda sonora personalizada), como un vídeo MP4 en videos junto a tus partidas guardadas, hasta esta fila otra vez (Parar la grabación) o Alt+F12. La imagen es como la toman las capturas, con las lecturas, sin las líneas de abajo; el tiempo del vídeo es el del juego: parado o en este menú no se graba nada, y ralentizado o acelerado se reproduce a la velocidad propia del juego."},
    {"Video size", "Tamaño del vídeo"},
    {"How large Record video makes the picture: so many times the board's lines, each of its pixels a sharp block, in the shape the Display page shows it in.", "Qué tamaño da Grabar vídeo a la imagen: tantas veces las líneas de la placa, cada uno de sus píxeles un bloque nítido, con la forma en que la muestra la página Pantalla."},
    {"3 times (672 lines)", "3 veces (672 líneas)"},
    {"2 times (448 lines)", "2 veces (448 líneas)"},
    {"4 times (896 lines)", "4 veces (896 líneas)"},
    {"1080 lines", "1080 líneas"},
    {"Show videos folder", "Mostrar carpeta de vídeos"},
    {"Opens videos beside your saves, where Record video writes, in your file manager.", "Abre videos junto a tus partidas guardadas, donde escribe Grabar vídeo, en tu gestor de archivos."},
    {"Stop recording video", "Parar la grabación de vídeo"},
    {"Ends the video being recorded and saves it.", "Termina el vídeo que se está grabando y lo guarda."},
    {"RECORDING VIDEO", "GRABANDO VÍDEO"},
    {"VIDEO SAVED", "VÍDEO GUARDADO"},
    {"VIDEO NOT SAVED", "VÍDEO NO GUARDADO"},
    {"30 seconds", "30 segundos"},
    {"10 seconds", "10 segundos"},
    {"Save replay", "Guardar repetición"},
    {"Saves what was just played, as long as Replay length says, as a video in videos (as Record video's, its sound too): kept all the while, so it needs no recording started. Made beside the game in a few seconds, which goes on meanwhile. Also Alt+Shift+F12.", "Guarda lo que se acaba de jugar, tanto como diga Duración de la repetición, como un vídeo en videos (como el de Grabar vídeo, también con su sonido): se guarda todo el tiempo, así que no hace falta empezar a grabar. Se hace junto al juego en unos segundos, y el juego sigue mientras. También Alt+Shift+F12."},
    {"Replay length", "Duración de la repetición"},
    {"How much of what was just played Save replay saves. Kept as a save state every second (what each has the same as the one before kept once) and the controls of every frame: 30 seconds of it is about 40 megabytes of memory, 60 on another game's stage. Not online.", "Cuánto de lo que se acaba de jugar guarda Guardar repetición. Se guarda como un estado cada segundo (lo que cada uno tiene igual que el anterior, una sola vez) y los controles de cada frame: 30 segundos son unos 40 megabytes de memoria, 60 en el escenario de otro juego. No en línea."},
    {"60 seconds", "60 segundos"},
    {"Off", "No"},
    {"A REPLAY IS STILL BEING SAVED", "TODAVÍA SE ESTÁ GUARDANDO UNA REPETICIÓN"},
    {"NOTHING TO REPLAY YET", "AÚN NO HAY NADA QUE REPETIR"},
    {"REPLAY LENGTH IS OFF", "LA DURACIÓN DE LA REPETICIÓN ESTÁ EN NO"},
    {"REPLAY NOT SAVED", "REPETICIÓN NO GUARDADA"},
    {"SAVING REPLAY...", "GUARDANDO REPETICIÓN..."},
    {"REPLAY SAVED", "REPETICIÓN GUARDADA"},
};

} // namespace port
