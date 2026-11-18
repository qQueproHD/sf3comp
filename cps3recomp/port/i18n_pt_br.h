// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); from cps2recomp.
//
// The port's own words in this language (i18n.h); cps2recomp's translations of the same words.
#pragma once

#include "../recomp-common/i18n.h"

namespace port {

inline constexpr recomp::i18n::Entry kPortuguese[] = {
    {"Video", "Vídeo"},
    {"What you play and hear, recorded as an MP4 video that plays anywhere.", "O que você joga e ouve, gravado como um vídeo MP4 que toca em qualquer lugar."},
    {"Record video", "Gravar vídeo"},
    {"Records the game as you play it, with its sound (the custom soundtrack's too), as an MP4 video in videos beside your saves, until this row again (Stop recording) or Alt+F12. The picture is as screenshots take it, readouts and all, without the lines at the bottom; the video's time is the game's: stopped or in this menu nothing is recorded, and slowed or sped up it plays at the game's own speed.", "Grava o jogo como você joga, com o som (também o da trilha sonora personalizada), como um vídeo MP4 em videos ao lado dos seus saves, até esta linha de novo (Parar gravação) ou Alt+F12. A imagem é como as capturas a tiram, com as leituras, sem as linhas de baixo; o tempo do vídeo é o do jogo: parado ou neste menu nada é gravado, e mais lento ou mais rápido ele toca na velocidade do próprio jogo."},
    {"Video size", "Tamanho do vídeo"},
    {"How large Record video makes the picture: so many times the board's lines, each of its pixels a sharp block, in the shape the Display page shows it in.", "O tamanho que Gravar vídeo dá à imagem: tantas vezes as linhas da placa, cada pixel um bloco nítido, no formato em que a página Tela a mostra."},
    {"3 times (672 lines)", "3 vezes (672 linhas)"},
    {"2 times (448 lines)", "2 vezes (448 linhas)"},
    {"4 times (896 lines)", "4 vezes (896 linhas)"},
    {"1080 lines", "1080 linhas"},
    {"Show videos folder", "Mostrar pasta de vídeos"},
    {"Opens videos beside your saves, where Record video writes, in your file manager.", "Abre videos ao lado dos seus saves, onde Gravar vídeo escreve, no seu gerenciador de arquivos."},
    {"Stop recording video", "Parar gravação de vídeo"},
    {"Ends the video being recorded and saves it.", "Termina o vídeo sendo gravado e o salva."},
    {"RECORDING VIDEO", "GRAVANDO VÍDEO"},
    {"VIDEO SAVED", "VÍDEO SALVO"},
    {"VIDEO NOT SAVED", "VÍDEO NÃO SALVO"},
    {"30 seconds", "30 segundos"},
    {"10 seconds", "10 segundos"},
    {"Save replay", "Salvar replay"},
    {"Saves what was just played, as long as Replay length says, as a video in videos (as Record video's, its sound too): kept all the while, so it needs no recording started. Made beside the game in a few seconds, which goes on meanwhile. Also Alt+Shift+F12.", "Salva o que acabou de ser jogado, pelo tempo que Duração do replay diz, como um vídeo em videos (como o de Gravar vídeo, com o som também): é guardado o tempo todo, então não é preciso começar a gravar. Feito ao lado do jogo em alguns segundos, e o jogo continua enquanto isso. Também Alt+Shift+F12."},
    {"Replay length", "Duração do replay"},
    {"How much of what was just played Save replay saves. Kept as a save state every second (what each has the same as the one before kept once) and the controls of every frame: 30 seconds of it is about 40 megabytes of memory, 60 on another game's stage. Not online.", "Quanto do que acabou de ser jogado Salvar replay salva. Guardado como um estado a cada segundo (o que cada um tem igual ao anterior, uma vez só) e os controles de cada frame: 30 segundos são uns 40 megabytes de memória, 60 no cenário de outro jogo. Não online."},
    {"60 seconds", "60 segundos"},
    {"Off", "Desligado"},
    {"A REPLAY IS STILL BEING SAVED", "UM REPLAY AINDA ESTÁ SENDO SALVO"},
    {"NOTHING TO REPLAY YET", "AINDA NÃO HÁ NADA PARA O REPLAY"},
    {"REPLAY LENGTH IS OFF", "A DURAÇÃO DO REPLAY ESTÁ DESLIGADA"},
    {"REPLAY NOT SAVED", "REPLAY NÃO SALVO"},
    {"SAVING REPLAY...", "SALVANDO REPLAY..."},
    {"REPLAY SAVED", "REPLAY SALVO"},
};

} // namespace port
