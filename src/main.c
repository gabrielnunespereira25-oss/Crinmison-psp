
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>

PSP_MODULE_INFO("Crinmison", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int main(void)
{
    pspDebugScreenInit();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    pspDebugScreenSetTextColor(0xFFFFFFFF);

    pspDebugScreenPrintf("CRINMISON PSP\n\n");
    pspDebugScreenPrintf("Motor do jogo iniciado!\n");
    pspDebugScreenPrintf("Preparando o mundo...\n\n");

    pspDebugScreenPrintf("CONTROLES:\n");
    pspDebugScreenPrintf("D-Pad: Movimento\n");
    pspDebugScreenPrintf("X: Pular\n");
    pspDebugScreenPrintf("Quadrado: Ataque\n");
    pspDebugScreenPrintf("Circulo: Magia\n");
    pspDebugScreenPrintf("START: Pausar\n");

    while (1)
    {
        sceDisplayWaitVblankStart();
    }

    return 0;
}
