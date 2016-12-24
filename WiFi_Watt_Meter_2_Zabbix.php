<?php
// Cfg
//$configUrl = 'http://10.1.2.58';
$configUrl = 'http://10.1.1.140';
$configHostName = 'wifi-watt-meter';
$configZabbixSenderCmd = 'zabbix_sender';
$configZabbixHost = '127.0.0.1';

$k[] = 'volt';
$k[] = 'ampere';
$k[] = 'watt';
$k[] = 'freeHeap';
$k[] = 'uptime';
$k[] = 'clientHandleForced';
// End Cfg

$curl = curl_init();
curl_setopt($curl, CURLOPT_URL, $configUrl);
curl_setopt($curl, CURLOPT_TIMEOUT, 5);
curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
$jsonRaw = curl_exec($curl);
curl_close($curl);

if ($jsonRaw === false) {
    echo "Failed to get: $configUrl\n";
} else {
    $json = json_decode($jsonRaw, true);

    if ($json === NULL) {
        echo "Failed to decode json";
        exit(1);
    } else {
        $kv = '';
        $count_k = count($k);

        for ($i = 0; $i < $count_k; $i++) {

            if ($json[$k[$i]] == '')
                $json[$k[$i]] = 0;

            $kv .= $configHostName . ' ' . $k[$i] . ' ' . $json[$k[$i]];

            if ($i != $count_k - 1)
                $kv .= "\n";
        }

        $cmd = "echo \"" . $kv . "\" | " . $configZabbixSenderCmd . ' -z ' . $configZabbixHost . ' -i -';
        echo "Run: $cmd\n";
        exec($cmd, $output, $ret);

        echo 'Output: ';
        print_r($output);

        if ($ret !== 0) {
            echo "Failed with the exit code: $ret\n";
            exit($ret);
        }
    }
}
