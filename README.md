# Chytrá zásuvka s ESP32

## Hlavní funkce

* **Vzdálené ovládání:** Zapínání a vypínání spotřebičů odkudkoli prostřednictvím webového rozhraní.
* **Monitorování spotřeby:** Zobrazení aktuálního napětí (V), proudu (A) a příkonu (W).
* **Autonomní provoz:** Zařízení hostuje vlastní Wi-Fi síť a webový server, nepotřebuje připojení k internetu.
* **Přesná měření:** Softwarové filtry eliminují falešná měření a zajišťují přesné údaje i při vypnutém stavu.

## Instalace a nastavení

1.  **Zapojení do sítě:** Zapojte prodlužovací kabel do standardní síťové zásuvky (230V AC). Po zapojení by se mělo zařízení automaticky spustit.
2.  **Připojení k Wi-Fi:** Na svém telefonu, tabletu nebo počítači vyhledejte dostupné Wi-Fi sítě. Připojte se k síti s názvem **"Smart_Outlet_Monitor"**.
3.  **Zadání hesla:** Po výzvě zadejte heslo k Wi-Fi síti: **"password123"**.

## Ovládání a navigace

1.  **Otevření webového rozhraní:** Jakmile jste připojeni k Wi-Fi síti zařízení, otevřete webový prohlížeč a do adresního řádku zadejte: **`http://192.168.4.1`**.
2.  **Dashboard:** Načte se hlavní stránka aplikace, která přehledně zobrazuje všechny monitorované údaje.
3.  **Vzdálené spínání:** Pro zapnutí nebo vypnutí připojeného spotřebiče jednoduše klikněte na tlačítko **"TURN ON"** (pro zapnutí) nebo **"TURN OFF"** (pro vypnutí).
4.  **Monitorovací údaje:** Na dashboardu uvidíte v reálném čase tyto údaje:
    * **Napětí:** Aktuální napětí v síti (V).
    * **Proud:** Aktuální odebíraný proud spotřebičem (A).
    * **Výkon:** Vypočítaný příkon (W).
5.  **Obnovení:** Dashboard se automaticky obnovuje každých několik sekund, abyste měli k dispozici nejaktuálnější data.
