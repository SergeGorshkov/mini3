<?php
ini_set("display_errors", "1");

function request( $method, $endpoint, $query ) {
    $context = stream_context_create( array(
        'http' => array(
            'method' => $method,
            'header' => 'Content-Type: application/x-www-form-urlencoded' . PHP_EOL,
            'content' => 'request=' . json_encode( $query ),
        ),
    ) );
    $response = file_get_contents( "http://localhost:8081/" . $endpoint, false, $context );
//echo("Return: ".$response."\n");
    return json_decode( $response );
}

// Отправляем префиксы

$context = stream_context_create( array(
    'http' => array(
        'method' => 'POST',
        'header' => 'Content-Type: application/x-www-form-urlencoded' . PHP_EOL,
        'content' => 'request=' . file_get_contents("prefixes.json"),
    ),
) );

echo("PUT prefixes\n");
$st = microtime(true);
$response = file_get_contents( "http://localhost:8081/prefix", false, $context );
$et = microtime(true);
print_r(json_decode( $response ));
echo(($et-$st)."\n");


echo("PUT triples\n");
$st = microtime(true);
for( $i=0; $i<10000; $i++ ) {
if($i % 100 == 0) echo("PUT ".$i."\n");
$req = [    'RequestId' => "req".$i,
            'Pattern' => []];
for( $j=0; $j<100; $j++)
	$req[ 'Pattern' ][] = [ 'Serge'.($i*100+$j), 'knows'.rand(), 'PHP'.rand() ];
$sti = microtime(true);
request( 'PUT', 'triple', $req );
$eti = microtime(true);
}
echo("\nLast insert time: ".($eti-$sti)."\n");

/*
// Удаляем триплеты
echo("DELETE triples\n");
$req = [    'RequestId' => '2',
	    'Pattern' => [
                [ '*', $knows, $php ]
            ]
            ];
print_r($req);

$st = microtime(true);
print_r(request( 'DELETE', 'triple', $req ));
$et = microtime(true);
echo("delete: ".($et-$st)."\n");
*/
$et = microtime(true);
echo("total: ".($et-$st)."\n");

// Получаем оставшиеся триплеты
/*
$req = [    'RequestId' => '3',
	    'Pattern' => [
                [ 'http://localhost/1402837773168899146218873503', 'http://localhost/1402837773168899146218873503', 'http://localhost/1402837773168899146218873503' ]
            ]
            ];
*/
$req['Pattern'] = [$req['Pattern'][0]];
echo("GET triples\n");
$st = microtime(true);
print_r(request( 'GET', 'triple', $req ));
$et = microtime(true);
echo("Get time: ".($et-$st)."\n");


?>