<?php

function request( $method, $endpoint, $query ) {
    $context = stream_context_create( array(
        'http' => array(
            'method' => $method,
            'header' => 'Content-Type: application/x-www-form-urlencoded' . PHP_EOL,
            'content' => 'request=' . json_encode( $query ),
        ),
    ) );
    $response = file_get_contents( "http://localhost:8082/" . $endpoint, false, $context );
//echo("Return: ".$response."\n");
    return json_decode( $response );
}

// Send prefixes
echo("1. PUT prefixes:\t\t\t");
$req = [ 'RequestId' => '1',
	 'Prefix' => [
		    [ '' => 'http://localhost/' ],
		    [ 'rdf' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#' ],
		    [ 'rdfs' => 'http://www.w3.org/2000/01/rdf-schema#' ],
		    [ 'owl' => 'http://www.w3.org/2002/07/owl#' ],
		    [ 'xsd' => 'http://www.w3.org/2001/XMLSchema#' ],
		    [ 'foaf' => 'http://xmlns.com/foaf/0.1/' ],
		    [ 'custom' => 'http://some.prefix/' ] ] ];
$st = microtime(true);
$res = request( 'PUT', 'prefix', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR! ".print_r($res, true));
echo("\t".round($et-$st,3)."\n");

// Delete one prefix
echo("2. DELETE prefixes:\t\t\t");
$req = [ 'RequestId' => '2',
	 'Prefix' => [
		    [ 'custom' => 'http://some.prefix' ] ] ];
$st = microtime(true);
$res = request( 'DELETE', 'prefix', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR! ".print_r($res, true));
echo("\t".round($et-$st,3)."\n");

// Get remaining prefixes
echo("3. GET prefixes:\t\t\t");
$st = microtime(true);
$res = request( 'GET', 'prefix', [ 'RequestId' => '3' ] );
$et = microtime(true);
if($res->Status == 'Ok' && isset($res->Prefixes) && sizeof($res->Prefixes) == 6) echo("OK");
else echo("ERROR! ".print_r($res, true));
echo("\t".round($et-$st,3)."\n");

// Send some triples
$req = [    'RequestId' => '4',
	    'Pattern' => [
                [ 'Jack', 'knows', 'PHP' ],
                [ 'Jack', 'knows', 'JavaScript' ],
                [ 'Jack', 'knows', 'C++' ],
                [ 'Jane', 'knows', 'C++' ],
                [ 'John', 'knows', 'PHP' ],
            ]
            ];
echo("4. PUT triples:\t\t\t\t");
$st = microtime(true);
$res = request( 'PUT', 'triple', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR! ".print_r($res, true));
echo("\t".round($et-$st,3)."\n");

// Delete some triples
echo("5. DELETE triples:\t\t\t");
$req = [    'RequestId' => '5',
	    'Pattern' => [
                [ '*', 'knows', 'JavaScript' ]
            ]
            ];

$st = microtime(true);
$res = request( 'DELETE', 'triple', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR! ".print_r($res, true));
echo("\t".round($et-$st,3)."\n");

// Get deleted triples (shall return nothing)
$req = [    'RequestId' => '6',
	    'Pattern' => [
                [ 'Jack', 'knows', 'JavaScript' ]
            ]
            ];
echo("6. GET deleted triples:\t\t\t");
$st = microtime(true);
$res = request( 'GET', 'triple', $req );
$et = microtime(true);
if($res->Status == 'Ok' && !isset($res->Triples)) echo("OK");
else echo("ERROR! ".print_r($res, true));
echo("\t".round($et-$st,3)."\n");

// Get triples count
$req = [    'RequestId' => '7',
	    'Pattern' => [
                [ '*', '*', '*' ]
            ]
            ];
echo("7. GET triples count:\t\t\t");
$st = microtime(true);
$res = request( 'GET', 'triple/count', $req );
$et = microtime(true);
if($res->Status == 'Ok' && $res->Count == 4) echo("OK");
else echo("ERROR! ".print_r($res, true));
echo("\t".round($et-$st,3)."\n");

// Get the remaining triples
$req = [    'RequestId' => '8',
	    'Pattern' => [
                [ '*', '*', '*' ]
            ]
            ];
echo("8. GET remaining triples:\t\t");
$st = microtime(true);
$res = request( 'GET', 'triple', $req );
$et = microtime(true);
if($res->Status == 'Ok' && sizeof($res->Triples) == 4) echo("OK");
else echo("ERROR! ".print_r($res, true));
echo("\t".round($et-$st,3)."\n");

// Get triples by pattern with sorting
echo("9. GET triples with sorting:\t\t");
$req = [	'RequestId' => '9',
		'Pattern' => [
			[ '*', 'knows', '*' ]
		],
		'Order' => [
			[ 'subject', 'desc' ],
			[ 'object', 'asc' ]
		]
	];
$st = microtime(true);
$res = request('GET', 'triple', $req );
$et = microtime(true);
if(!isset($res->Triples) || sizeof($res->Triples) != 4 || $res->Triples[0][0] != 'http://localhost/John' || $res->Triples[1][0] != 'http://localhost/Jane' || $res->Triples[2][0] != 'http://localhost/Jack' || $res->Triples[3][0] != 'http://localhost/Jack' || $res->Triples[2][2] != 'http://localhost/C++' || $res->Triples[3][2] != 'http://localhost/PHP')  echo("ERROR! ".print_r($res, true));
else echo("OK");
echo("\t".round($et-$st,3)."\n");

// Restore deleted triple
$req = [    'RequestId' => '10a',
	    'Pattern' => [
                [ 'Jack', 'knows', 'JavaScript' ]
            ]
            ];
echo("10. PUT triple (restore deleted):\t");
$st = microtime(true);
$res = request( 'PUT', 'triple', $req );
$et = microtime(true);
$req = [	'RequestId' => '10b',
		'Pattern' => [
			[ '*', 'knows', '*' ]
		]
	];
$st = microtime(true);
$res = request('GET', 'triple', $req );
$et = microtime(true);
if(!isset($res->Triples) || sizeof($res->Triples) != 5 || $res->Triples[0][0] != 'http://localhost/Jack')  echo("ERROR! ".print_r($res, true));
else echo("OK");
echo("\t".round($et-$st,3)."\n");

// Send some triples with the language, data type, strange characters
$inner_json = ['first_key'=>'value', 'second_key'=>['a' => 'b', 'c'=>'&nbsp;{["\'']];
$req = [    'RequestId' => '11a',
	    'Pattern' => [
                [ 'Jack', 'hasSignature', json_encode($inner_json), 'xsd:string' ],
                [ 'Jack', 'rdfs:label', 'Джек Крутоёлкин', 'xsd:string', 'RU' ],
                [ 'Jack', 'hasFavoriteBook', '"Незнайка на Луне", &#169; Н. Носов', 'xsd:string', 'RU' ]
            ]
            ];
echo("11. PUT triples with literals:\t\t");
$res = request( 'PUT', 'triple', $req );
$req = [	'RequestId' => '11b',
		'Pattern' => [
			[ 'Jack', '*', '*' ]
		]
	];
$st = microtime(true);
$res = request('GET', 'triple', $req );
$et = microtime(true);
if(!isset($res->Triples)) echo("ERROR! ".print_r($res, true));
else {
    $passed = true;
    foreach($res->Triples as $triple) {
	if($triple[1] == 'http://localhost/hasSignature') {
	    if($triple[2] != '{"first_key":"value","second_key":{"a":"b","c":"&nbsp;{[\"\'"}}' || $triple[3] != 'http://www.w3.org/2001/XMLSchema#string')
		$passed = false;
	}
	if($triple[1] == 'http://www.w3.org/2000/01/rdf-schema#label') {
	    if($triple[2] != 'Джек Крутоёлкин' || $triple[4] != 'RU')
		$passed = false;
	}
	if($triple[1] == 'http://localhost/hasFavoriteBook') {
	    if($triple[2] != '"Незнайка на Луне", &#169; Н. Носов' || $triple[4] != 'RU')
		$passed = false;
	}
    }
    if($passed) echo("OK");
    else echo("ERROR! ".print_r($res, true));
}
echo("\t".round($et-$st,3)."\n");

// Delete all triples
$req = [    'RequestId' => '50',
	    'Pattern' => [
                [ '*', '*', '*' ]
            ]
            ];
$res = request( 'DELETE', 'triple', $req );

?>
