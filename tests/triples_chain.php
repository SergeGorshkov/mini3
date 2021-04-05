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
echo("1. PUT prefixes\n");
$req = [ 'RequestId' => '1',
	 'Prefix' => [ 
		    [ '' => 'http://localhost/' ],
		    [ 'rdf' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#' ],
		    [ 'rdfs' => 'http://www.w3.org/2000/01/rdf-schema#' ],
		    [ 'owl' => 'http://www.w3.org/2002/07/owl#' ],
		    [ 'xsd' => 'http://www.w3.org/2001/XMLSchema#' ],
		    [ 'foaf' => 'http://xmlns.com/foaf/0.1/' ] ] ];
$st = microtime(true);
$res = request( 'PUT', 'prefix', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");

// Populate database with triples
$req = [    'RequestId' => '2',
	    'Pattern' => [
		[ 'Person', 'rdf:type', 'owl:Class' ],
		[ 'Person', 'rdfs:label', 'Person', 'xsd:string' ],
		[ 'Language', 'rdf:type', 'owl:Class' ],
		[ 'Language', 'rdfs:label', 'Language', 'xsd:string' ],
		[ 'Programmer', 'rdf:type', 'owl:Class' ],
		[ 'Programmer', 'rdfs:subClassOf', 'Person' ],
		[ 'Programmer', 'rdfs:label', 'Programmer', 'xsd:string' ],
		[ 'Designer', 'rdf:type', 'owl:Class' ],
		[ 'Designer', 'rdfs:subClassOf', 'Person' ],
		[ 'Designer', 'rdfs:label', 'Designer', 'xsd:string' ],
		[ 'LanguageType', 'rdf:type', 'owl:Class' ],
		[ 'LanguageType', 'rdfs:label', 'Language type', 'xsd:string' ],
		[ 'hasType', 'rdf:type', 'owl:ObjectProperty' ],
		[ 'hasType', 'rdfs:label', 'has type', 'xsd:string' ],
		[ 'hasType', 'rdfs:domain', 'Language' ],
		[ 'hasType', 'rdfs:range', 'LanguageType' ],
		[ 'yearOfBirth', 'rdf:type', 'owl:ObjectProperty' ],
		[ 'yearOfBirth', 'rdfs:label', 'has year of birth', 'xsd:string' ],
		[ 'yearOfBirth', 'rdfs:domain', 'Person' ],
		[ 'yearOfBirth', 'rdfs:range', 'xsd:integer' ],
		[ 'knows', 'rdf:type', 'owl:ObjectProperty' ],
		[ 'knows', 'rdfs:label', 'knows', 'xsd:string' ],
		[ 'knows', 'rdfs:domain', 'Person' ],
		[ 'knows', 'rdfs:range', 'Language' ],
		[ 'isKnownBy', 'rdf:type', 'owl:ObjectProperty' ],
		[ 'isKnownBy', 'rdfs:label', 'is known by', 'xsd:string' ],
		[ 'isKnownBy', 'rdfs:domain', 'Language' ],
		[ 'isKnownBy', 'rdfs:range', 'Person' ],

		[ 'Interpretable', 'rdf:type', 'LanguageType' ],
		[ 'Interpretable', 'rdfs:label', 'Interpretable', 'xsd:string' ],
		[ 'Interpretable', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'Compilable', 'rdf:type', 'LanguageType' ],
		[ 'Compilable', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'Compilable', 'rdfs:label', 'Compilable', 'xsd:string' ],
		[ 'Compilable', 'rdfs:label', 'Compilable to binary code', 'xsd:string' ],
		[ 'Jack', 'rdf:type', 'Person' ],
		[ 'Jack', 'rdf:type', 'Programmer' ],
		[ 'Jack', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'Jack', 'yearOfBirth', '1979', 'xsd:integer' ],
		[ 'Jack', 'rdfs:label', 'Jack Doe', 'xsd:string', 'EN' ],
		[ 'Jack', 'rdfs:label', 'Джек Доу', 'xsd:string', 'RU' ],
		[ 'Jane', 'rdf:type', 'Person' ],
		[ 'Jane', 'rdf:type', 'Programmer' ],
		[ 'Jane', 'rdf:type', 'Designer' ],
		[ 'Jane', 'yearOfBirth', '1985', 'xsd:integer' ],
		[ 'Jane', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'Jane', 'rdfs:label', 'Jane Doe', 'xsd:string', 'EN' ],
		[ 'Jill', 'rdf:type', 'Person' ],
		[ 'Jill', 'rdf:type', 'Designer' ],
		[ 'Jill', 'yearOfBirth', '1990', 'xsd:integer' ],
		[ 'Jill', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'Jill', 'rdfs:label', 'Jill Doe', 'xsd:string', 'EN' ],
		[ 'PHP', 'rdf:type', 'Language' ],
		[ 'PHP', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'PHP', 'rdfs:label', 'PHP', 'xsd:string' ],
		[ 'PHP', 'hasType', 'Interpretable' ],
		[ 'JavaScript', 'rdf:type', 'Language' ],
		[ 'JavaScript', 'rdfs:label', 'JavaScript', 'xsd:string' ],
		[ 'JavaScript', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'JavaScript', 'hasType', 'Interpretable' ],
		[ 'C++', 'rdf:type', 'Language' ],
		[ 'C++', 'rdfs:label', 'C++', 'xsd:string' ],
		[ 'C++', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'C++', 'hasType', 'Compilable' ],
                [ 'Jack', 'knows', 'PHP' ],
                [ 'Jack', 'knows', 'JavaScript' ],
                [ 'Jack', 'knows', 'C++' ],
                [ 'C++', 'isKnownBy', 'Jack' ],
                [ 'Jane', 'knows', 'C++' ],
                [ 'C++', 'isKnownBy', 'Jane' ],

		[ 'Project', 'rdf:type', 'owl:Class' ],
		[ 'Project', 'rdfs:label', 'Project' ,'xsd:string' ],
		[ 'participatesIn', 'rdf:type', 'owl:ObjectProperty' ],
		[ 'participatesIn', 'rdfs:domain', 'Person' ],
		[ 'participatesIn', 'rdfs:range', 'Project' ],
		[ 'participatesIn', 'rdfs:label', 'participates in', 'xsd:string' ],
		[ 'writtenOn', 'rdf:type', 'owl:ObjectProperty' ],
		[ 'writtenOn', 'rdfs:domain', 'Project' ],
		[ 'writtenOn', 'rdfs:range', 'Language' ],
		[ 'writtenOn', 'rdfs:label', 'is written on', 'xsd:string' ],

		[ 'ABC', 'rdf:type', 'Project' ],
		[ 'ABC', 'writtenOn', 'C++' ],
		[ 'ABC', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'Jack', 'participatesIn', 'ABC' ],
		[ 'Jane', 'participatesIn', 'ABC' ],
		[ 'DEF', 'rdf:type', 'Project' ],
		[ 'DEF', 'writtenOn', 'PHP' ],
		[ 'DEF', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'Jack', 'participatesIn', 'DEF' ]
            ]
            ];

echo("2. PUT triples\n");
$res = request( 'PUT', 'triple', $req );
if($res->Status == 'Ok') echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");

$req = [    'RequestId' => '3',
	    'Chain' => [
                [ 'Jack', 'knows', '?lang' ],
                [ '?lang', 'hasType', '?type' ],
                [ '?type', 'rdfs:label', '?typename' ],
                [ '?person', 'rdf:type', 'Person' ],
                [ '?person', 'rdfs:label', '?personname' ],
                [ '?lang', '?known', '?person' ],
            ],
            'Order' => [ [ '?person', 'desc' ],
        		 [ '?personname', 'asc' ]
            ],
            'Filter' => [
        	'and',
        	[ 'or',
        	    [ '?personname', 'contains', 'Jack' ],
        	    [ '?personname', 'contains', 'Сергей' ],
        	],
        	[ 'and',
        	    [ '?lang', 'equal', 'C++' ]
        	]
            ]
            ];

echo("3. GET chain\n");
$st = microtime(true);
$res = request('GET', 'chain', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");
//print_r($res);
//exit;

// Print table
foreach($res->Vars as $rr) {
    echo($rr."\t");
}
echo("\n");
foreach($res->Result as $rr) {
    foreach($rr as $r) {
	echo($r[0]."\t");
    }
    echo("\n");
}

$req = [    'RequestId' => '4',
	    'Chain' => [
                [ '?lang', 'rdf:type', 'Language' ],
                [ '?lang', 'isKnownBy', '?person' ],
            ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?person', 'notexists', '' ]
        	]
            ]
            ];

echo("\n4. GET chain with notexists filter\n");
$st = microtime(true);
$res = request('GET', 'chain', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");

// Print table
foreach($res->Vars as $rr) {
    echo($rr."\t");
}
echo("\n");
foreach($res->Result as $rr) {
    foreach($rr as $r) {
	echo($r[0]."\t");
    }
    echo("\n");
}

$tests = [

	[   'RequestId' => '0',
	    'Chain' => [
                [ '?lang', 'rdf:type', 'Language' ],
                [ '?lang', 'rdfs:label', 'C++', 'xsd:string' ],
                [ '?lang', 'isKnownBy', '?person' ]
            ]
        ],

	[   'RequestId' => '1',
	    'Chain' => [
                [ '?lang', 'rdf:type', 'Language' ],
                [ '?lang', '?a', '?b' ]
            ]
        ],

	[   'RequestId' => '2',
	    'Chain' => [
                [ '?person', 'rdf:type', 'Person' ],
                [ '?person', 'rdfs:label', '?name' ],
                [ '?person', 'knows', '?lang' ],
                [ '?person', 'participatesIn', '?project' ]
            ]
        ],

	[   'RequestId' => '3',
	    'Chain' => [
                [ '?lang', 'rdf:type', 'Language' ],
                [ '?lang', '?property', '?name' ],
            ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?property', 'equal', 'rdfs:label' ]
        	]
            ]
        ],

	[   'RequestId' => '6',
	    'Chain' => [
                [ '?person1', 'rdf:type', 'Person' ],
                [ '?person2', 'rdf:type', 'Person' ],
                [ '?person1', 'participatesIn', '?project' ],
                [ '?person2', 'participatesIn', '?project' ],
                [ '?person1', 'knows', '?lang1' ],
                [ '?person2', 'knows', '?lang2' ],
                [ '?lang1', 'hasType', '?type1' ],
                [ '?lang2', 'hasType', '?type2' ],
            ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?person1', 'notequal', '?person2' ]
        	]
            ]
        ],

	[   'RequestId' => '7',
	    'Chain' => [
                [ '?person', 'rdf:type', 'Person' ],
                [ '?person', 'knows', '?lang1' ],
                [ '?project', 'rdf:type', 'Project' ],
                [ '?project', 'writtenOn', '?lang2' ],
                [ '?lang1', 'rdfs:label', '?name1' ],
                [ '?lang2', 'rdfs:label', '?name2' ],
            ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?name1', 'equal', '?name2' ]
        	]
            ]
        ],

	[   'RequestId' => '8',
	    'Chain' => [
                [ '?person', 'rdf:type', 'Person' ],
                [ '?person', 'knows', '?lang1' ],
                [ '?project', 'rdf:type', 'Project' ],
                [ '?project', 'writtenOn', '?lang2' ],
                [ '?lang1', 'rdfs:label', '?name1' ],
            ],
        ],

	[   'RequestId' => '9',
	    'Chain' => [
                [ '?programmer', 'rdfs:subClassOf', 'Person' ],
                [ '?person', 'rdf:type', '?programmer' ],
                [ '?person', 'rdfs:label', '?name' ],
		[ '?person', 'yearOfBirth', '?year' ],
                [ '?person', 'knows', '?lang' ],
                [ '?lang', 'rdfs:label', '?namelang' ],
                [ '?subclass', 'rdfs:subClassOf', 'Person' ],
                [ '?person2', 'rdf:type', '?subclass' ],
                [ '?person2', 'rdfs:label', '?name2' ],
		[ '?person2', 'yearOfBirth', '?year2' ],
            ],
	    'Optional' => [ '?subclass', '?person2', '?year2' ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?year', 'more', '?year2' ]
        	]
            ]
        ],

];

$answers = [
	    [ 'hasResult' =>
	      [ [ '?lang' => 'C++', '?person' => 'Jack' ],
	        [ '?lang' => 'C++', '?person' => 'Jane' ] ],
	      'numRecords' => 2 ],

	    [ 'hasResult' =>
	      [ [ '?lang' => 'C++', '?a' => 'rdfs:label', '?b' => 'C++' ],
	        [ '?lang' => 'C++', '?a' => 'hasType', '?b' => 'Compilable' ],
	        [ '?lang' => 'C++', '?a' => 'rdf:type', '?b' => 'Language' ],
	        [ '?lang' => 'C++', '?a' => 'rdf:type', '?b' => 'owl:NamedIndividual' ] ],
	      'numRecords' => 14 ],

	    [ 'hasResult' =>
	      [ [ '?person' => 'Jack', '?name' => 'Jack Doe', '?lang' => 'C++', '?project' => 'ABC' ],
	        [ '?person' => 'Jack', '?name' => 'Jack Doe', '?lang' => 'C++', '?project' => 'DEF' ]
	        [ '?person' => 'Jack', '?name' => 'Джек Доу', '?lang' => 'JavaScript', '?project' => 'DEF' ]
	        [ '?person' => 'Jack', '?name' => 'Jack Doe', '?lang' => 'JavaScript', '?project' => 'ABC' ]
	        [ '?person' => 'Jack', '?name' => 'Jack Doe', '?lang' => 'PHP', '?project' => 'DEF' ] ],
	     'numRecords' => 25 ],

	    [ 'hasResult' =>
	      [ [ '?lang' => 'C++', '?property' => 'rdfs:label', '?name' => 'C++' ] 
	        [ '?lang' => 'PHP', '?property' => 'rdfs:label', '?name' => 'PHP' ] 
	        [ '?lang' => 'JavaScript', '?property' => 'rdfs:label', '?name' => 'JavaScript' ] ],
	      'numRecords' => 3 ],

	    [ 'hasResult' =>
	      [ [ '?person1' => 'Jack', '?project' => 'ABC', '?lang1' => 'C++', '?type1' => 'Compilable', '?person2' => 'Jane', '?lang2' => 'C++', '?type2' => 'Compilable' ],
	        [ '?person1' => 'Jane', '?lang1' => 'C++', '?type1' => 'Compilable', '?person2' => 'Jack', '?project' => 'ABC', '?lang2' => 'PHP', '?type1' => 'Interpretable' ] ],
	      'numRecords' => 4 ],

	    [ 'hasResult' => 
	      [ [ '?person' => 'Jack', '?lang1' => 'PHP', 'name1' => 'PHP', '?project' => 'ABC', '?lang2' => 'C++' ],
	        [ '?person' => 'Jack', '?lang1' => 'C++', 'name1' => 'C++', '?project' => 'DEF', '?lang2' => 'PHP' ],
	        [ '?person' => 'Jack', '?lang1' => 'JavaScript', 'name1' => 'JavaScript', '?project' => 'ABC', '?lang2' => 'C++' ] ],
	      'numRecords' => 7 ],

	    [ 'hasResult' =>
	      [ [ '?programmer' => 'Programmer', '?person' => 'Jack', '?name' => 'Jack Doe', '?year' => '1979', '?lang' => 'PHP', '?subclass' => 'Designer', '?persons2' => 'Jill', 'name2' => 'Jill Doe', '?year2' => '1990' ],
	        [ '?programmer' => 'Programmer', '?person' => 'Jane', '?name' => 'Jane Doe', '?year' => '1985', '?lang' => 'C++', '?subclass' => 'Designer', '?persons2' => 'Jack', 'name2' => 'Jack Doe', '?year2' => '1979' ] ],
	      'numRecords' => 9 ]

	];


echo("\n5. Running complex tests\n");
foreach( $tests as $ind => $test ) {
	echo("\n" . $ind . ". ");
	$st = microtime(true);
	$res = request('GET', 'chain', $test );
	$et = microtime(true);
	if($res->Status == 'Ok') echo("OK");
	else echo("ERROR!");
	echo(" ".round($et-$st,3)."\n");

	$error = false;
	foreach( $answers[ $ind ][ 'hasResult' ] as $result ) {
		foreach( $res->Result as $rr ) {
			if( sizeof( $result ) != sizeof( $rr ) ) {
				echo( "Result size mismatch: " . sizeof( $rr ) . " instead of " . sizeof( $result ) . "\n");
				$error = true;
				break;
			}
		if( sizeof( array_diff_assoc( $rr, $result ) ) || sizeof( array_diff_assoc( $result, $rr ) ) ) {
			echo( "Result mismatch:\n" . print_r( $rr, true ) . "\n" . print_r( $result, true ) . "\n" );
			$error = true;
		}
	}

	if( sizeof($res->Result) != $answers[ $ind ][ 'numRecords' ] ) {
		echo( "Number of records mismatch: " . sizeof($res->Result) . " instead of " . $answers[ $ind ][ 'numRecords' ] . "\n" );
		$error = true;
	}

	if( $error ) {
		// Print table
		foreach($res->Vars as $rr) {
		    echo($rr."\t");
		}
		echo("\n");
		foreach($res->Result as $rr) {
		    foreach($rr as $r) {
			echo($r[0]."\t");
		    }
		    echo("\n");
		}
	}
}

?>
