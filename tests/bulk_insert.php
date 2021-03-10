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

// Send prefixes
echo("PUT prefixes\n");
$req = [ 'RequestId' => '1',
	 'Prefix' => [ 
		    [ '' => 'http://localhost/' ],
		    [ 'rdf' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#' ],
		    [ 'rdfs' => 'http://www.w3.org/2000/01/rdf-schema#' ],
		    [ 'owl' => 'http://www.w3.org/2002/07/owl#' ],
		    [ 'xsd' => 'http://www.w3.org/2001/XMLSchema#' ],
		    [ 'foaf' => 'http://xmlns.com/foaf/0.1/' ] ] ];
$st = microtime(true);
print_r(request( 'PUT', 'prefix', $req ));
$et = microtime(true);
echo(($et-$st)."\n");

// Send triples
$range = 1; $portion = 100;
echo("PUT " . ($range * $portion) . " triples\n");
$st = microtime(true);
for( $i=0; $i<$range; $i++ ) {
	if($i % 100 == 0) echo("PUT ".$i."\n");
	$req = [    'RequestId' => "req".$i,
	            'Pattern' => []];
	for( $j=0; $j<100; $j++)
		$req[ 'Pattern' ][] = [ 'Subject'.($i*100+$j), 'Predicate'.rand(), 'Object'.rand() ];
	$sti = microtime(true);
	request( 'PUT', 'triple', $req );
	$eti = microtime(true);
}
echo("\nLast " . $portion . " items insert time: ".($eti-$sti)."\n");
$pattern1 = [ '*', $req[ 'Pattern' ][ sizeof($req['Pattern']) - 1 ][1], $req[ 'Pattern' ][ sizeof($req['Pattern']) - 1 ][2] ];
$pattern2 = [ $req[ 'Pattern' ][ sizeof($req['Pattern']) - 2 ][0], '*', $req[ 'Pattern' ][ sizeof($req['Pattern']) - 2 ][2] ];

// Delete one triple
echo("DELETE triples\n");
$req = [    'RequestId' => '2',
	    'Pattern' => [
                $pattern1
            ]
            ];
print_r($req);
$st = microtime(true);
request( 'DELETE', 'triple', $req );
$et = microtime(true);
echo("Delete time: ".($et-$st)."\n");

// Get amount of triples by pattern
$req = [    'RequestId' => '3',
	    'Pattern' => [
                $pattern2
            ]
            ];
echo("GET triples count\n");
$st = microtime(true);
print_r(request( 'GET', 'triple/count', $req ));
$et = microtime(true);
echo("Get count time: ".($et-$st)."\n");

// Get triples by pattern
$req = [    'RequestId' => '3',
	    'Pattern' => [
                $pattern2
            ]
            ];
echo("GET triples\n");
$st = microtime(true);
print_r(request( 'GET', 'triple', $req ));
$et = microtime(true);
echo("Get time: ".($et-$st)."\n");

?>