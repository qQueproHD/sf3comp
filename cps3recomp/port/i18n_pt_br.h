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
};

} // namespace port
