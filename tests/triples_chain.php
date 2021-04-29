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

/*
$req = [    'RequestId' => '3',
	    'Chain' => [
                [ '?a', 'http://www.w3.org/1999/02/22-rdf-syntax-ns#type', '?type' ],
                [ '?a', 'http://trinidata.ru/archigraph-mdm/SourceSystem', '?field_0_0' ],
            ],
            'Order' => [ [ '?type', 'asc' ],
        	[ '?a', 'desc' ]
            ],
            'Filter' => [
        	'and',
        	[ 'or',
        	    [ '?type', 'equal', 'http://localhost/Person' ],
        	    [ '?type', 'equal', 'http://localhost/Programmer' ],
        	],
        	[ 'and',
        	    [ '?field_0_0', 'notexists', '' ]
        	]
            ]
            ];

$st = microtime(true);
$res = request('GET', 'chain', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");
print_r($res);
exit;
*/

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
		[ 'Jack', 'worksSince', '2015-05-23', 'xsd:date' ],
		[ 'Jack', 'lastVisit', '2021-04-12T15:24:45', 'xsd:datetime' ],
		[ 'Jack', 'rdfs:label', 'Jack Doe', 'xsd:string', 'EN' ],
		[ 'Jack', 'rdfs:label', 'Джек Доу', 'xsd:string', 'RU' ],
		[ 'Jax', 'rdf:type', 'Programmer' ],
		[ 'Jax', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'Jax', 'yearOfBirth', '1979', 'xsd:integer' ],
		[ 'Jax', 'worksSince', '2019-01-18', 'xsd:date' ],
		[ 'Jane', 'rdf:type', 'Person' ],
		[ 'Jane', 'rdf:type', 'Programmer' ],
		[ 'Jane', 'rdf:type', 'Designer' ],
		[ 'Jane', 'yearOfBirth', '1985', 'xsd:integer' ],
		[ 'Jane', 'worksSince', '2016-12-18', 'xsd:date' ],
		[ 'Jane', 'lastVisit', '2021-04-12T12:21:23', 'xsd:datetime' ],
		[ 'Jane', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'Jane', 'rdfs:label', 'Jane Doe', 'xsd:string', 'EN' ],
		[ 'Jill', 'rdf:type', 'Person' ],
		[ 'Jill', 'rdf:type', 'Designer' ],
		[ 'Jill', 'yearOfBirth', '1990', 'xsd:integer' ],
		[ 'Jill', 'worksSince', '2015-05-18', 'xsd:date' ],
		[ 'Jill', 'lastVisit', '2021-04-12T15:28:12', 'xsd:datetime' ],
		[ 'Jill', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'Jill', 'rdfs:label', 'Jill Doe', 'xsd:string', 'EN' ],
		[ 'Jill', 'usesSoftware', 'Photoshop' ],
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

		[ 'Photoshop', 'rdf:type', 'Software' ],
		[ 'Photoshop', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'ABC', 'rdf:type', 'Project' ],
		[ 'ABC', 'writtenOn', 'C++' ],
		[ 'ABC', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'ABC', 'rdfs:label', 'ABC', 'xsd:string' ],
		[ 'Jack', 'participatesIn', 'ABC' ],
		[ 'Jane', 'participatesIn', 'ABC' ],
		[ 'DEF', 'rdf:type', 'Project' ],
		[ 'DEF', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'DEF', 'writtenOn', 'PHP' ],
		[ 'DEF', 'rdfs:label', 'DEF', 'xsd:string' ],
		[ 'DEF', 'rdf:type', 'owl:NamedIndividual' ],
		[ 'Jack', 'participatesIn', 'DEF' ]
            ]
            ];

echo("2. PUT triples\n");
$res = request( 'PUT', 'triple', $req );
if($res->Status == 'Ok') echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");

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
                [ '?person', 'worksSince', '?since' ],
                [ '?person', 'lastVisit', '?visit' ],
                [ '?person', 'yearOfBirth', '?year' ],
                [ [ [ '?person', 'participatesIn', '?project' ],
                    [ '?person', 'knows', '?lang' ] ],
		  [ [ '?person', 'usesSoftware', '?software' ] ] ]
            ],
            'Optional' => [ '?project', '?lang', '?software' ]
        ],

	[   'RequestId' => '3',
	    'Chain' => [
                [ '?person', 'rdf:type', 'Person' ],
                [ '?person', 'rdfs:label', '?name' ],
                [ '?person', 'knows', '?lang' ],
                [ '?person', 'participatesIn', '?project' ]
            ]
        ],

	[   'RequestId' => '4',
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

	[   'RequestId' => '5',
	    'Chain' => [
                [ '?person', 'rdf:type', '?class' ],
                [ '?person', '?property', '?lang' ],
            ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?class', 'equal', 'Programmer' ],
        	    [ '?property', 'equal', 'knows' ],
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
		[ '?person', 'worksSince', '?works' ],
                [ '?person', 'knows', '?lang' ],
                [ '?lang', 'rdfs:label', '?namelang' ],
                [ '?subclass', 'rdfs:subClassOf', 'Person' ],
                [ '?person2', 'rdf:type', '?subclass' ],
                [ '?person2', 'rdfs:label', '?name2' ],
                [ '?person2', 'knows', '?lang2' ],
		[ '?person2', 'yearOfBirth', '?year' ],
		[ '?person2', 'worksSince', '?works2' ],
            ],
	    'Optional' => [ [ '?lang2', '?name2' ] ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?works2', 'more', '?works' ]
        	]
            ]
        ],

	[   'RequestId' => '10',
	    'Chain' => [
                [ '?project', 'rdf:type', 'Project' ],
                [ '?project', 'rdfs:label', 'DEF', 'xsd:string' ],
                [ [ [ '?person', 'participatesIn', '?project' ],
                    [ '?person', 'participatesIn', '?otherproj' ],
                    [ '?person2', 'participatesIn', '?project' ],
                    [ '?person2', 'participatesIn', '?otherproj' ],
                    [ '?otherproj', 'rdfs:label', '?otherproj_label' ] ],
                  [ [ '?person', 'participatesIn', '?project' ],
                    [ '?person', 'participatesIn', '?otherproj' ],
                    [ '?otherproj', 'rdfs:label', '?otherproj_label' ] ] ],
            ],
	    'Optional' => [ '?person2' ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?otherproj', 'notequal', '?project' ]
        	]
            ]
        ],

	[   'RequestId' => '11',
	    'Chain' => [
                [ [ [ '?person', 'rdf:type', 'Person' ],
                    [ '?person', '?predicate', '?value' ] ],
                  [ [ '?person', 'rdf:type', 'Person' ],
                    [ '?person', '?predicate', '?object' ] ] ],
            ],
        ],

	[   'RequestId' => '12',
	    'Chain' => [
                [ '?subject', '?predicate', 'PHP' ],
            ],
        ],

	[   'RequestId' => '13',
	    'Chain' => [
                [ '?person', 'knows', '?lang' ],
            ],
	    'Order' => [ [ '?lang', 'asc' ] ],
	    'Limit' => 1,
	    'Offset' => 2
        ],

	[   'RequestId' => '14',
	    'Chain' => [
                [ '?object', 'rdfs:label', '?name' ],
            ],
	    'Order' => [ [ '?name', 'asc' ] ],
	    'Limit' => 1,
	    'Offset' => 2
        ],

	[   'RequestId' => '15',
	    'Chain' => [
                [ '?person', 'rdfs:label', '?name' ],
            ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?name', 'contains', 'Ja' ]
        	]
            ]
        ],

	[   'RequestId' => '16',
	    'Chain' => [
                [ '?person', 'knows', 'C++' ],
            ],
        ],

	[   'RequestId' => '17',
	    'Chain' => [
                [ '?person', 'rdf:type', 'Person' ],
                [ '?person', '?predicate', 'C++' ],
            ],
        ],

	[   'RequestId' => '18',
	    'Chain' => [
                [ '?person', 'rdf:type', 'Person' ],
                [ '?person', 'rdfs:label', '?name' ],
            ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?name', 'iequal', 'jack doe' ]
        	]
            ]
        ],

	[   'RequestId' => '19',
	    'Chain' => [
                [ '?person', 'rdf:type', 'Person' ],
                [ '?person', 'rdfs:label', '?name' ],
            ],
            'Filter' => [
        	'and',
        	[ 'or',
        	    [ '?name', 'iequal', 'jane doe' ],
        	    [ '?name', 'iequal', 'jack doe' ]
        	]
            ]
        ],

	[   'RequestId' => '20',
	    'Chain' => [
                [ '?project', 'rdf:type', 'Project' ],
                [ '?project', 'writtenOn', '?language' ],
                [ '?language', 'isKnownBy', '?person' ],
            ],
        ],

	[   'RequestId' => '21',
	    'Chain' => [
                [ '?person', 'rdf:type', 'Person' ],
                [ '?person', 'rdfs:label', '?name' ],
            ],
	    'Order' => [ [ '?name', 'asc' ] ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?name', 'contains', 'Ja' ]
        	]
            ],
	    'Limit' => 1,
	    'Offset' => 1
        ],

	[   'RequestId' => '22',
	    'Chain' => [
                [ '?person', 'rdf:type', 'Person' ],
                [ '?person', 'knows', '?lang' ],
                [ '?person', 'rdfs:label', '?name' ],
            ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?lang', 'notexists' ]
        	]
            ],
        ],

	[   'RequestId' => '23',
	    'Chain' => [
                [ '?person', 'rdf:type', 'Person' ],
                [ '?person', '?predicate', 'ABC' ],
            ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?predicate', 'notexists' ]
        	]
            ],
        ],

	[   'RequestId' => '24',
	    'Chain' => [
                [ '?person', 'rdf:type', 'Person' ],
		[ [ [ '?project', 'rdf:type', 'Project' ],
                    [ '?person', '?predicate', '?project' ] ],
		  [ [ '?software', 'rdf:type', 'Software' ],
                    [ '?person', '?predicate', '?software' ] ] ],
            ],
	    'Optional' => [ '?software', '?project' ]
        ],

	[   'RequestId' => '25',
	    'Chain' => [
                [ '?a', '?b', '?c' ],
            ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?c', 'icontains', 'ja' ]
        	]
            ]
        ],

];

$answers = [
	    [ 'hasResult' => // 0
	      [ [ '?lang' => 'http://localhost/C++', '?person' => 'http://localhost/Jack' ],
	        [ '?lang' => 'http://localhost/C++', '?person' => 'http://localhost/Jane' ] ],
	      'numRecords' => 2 ],

	    [ 'hasResult' => // 1
	      [ [ '?lang' => 'http://localhost/C++', '?a' => 'http://www.w3.org/2000/01/rdf-schema#label', '?b' => 'C++' ],
	        [ '?lang' => 'http://localhost/C++', '?a' => 'http://localhost/hasType', '?b' => 'http://localhost/Compilable' ],
	        [ '?lang' => 'http://localhost/C++', '?a' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#type', '?b' => 'http://localhost/Language' ],
	        [ '?lang' => 'http://localhost/C++', '?a' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#type', '?b' => 'http://www.w3.org/2002/07/owl#NamedIndividual' ] ],
	      'numRecords' => 14 ],

	    [ 'hasResult' => // 2
	      [ [ '?person' => 'http://localhost/Jack', '?name' => 'Jack Doe', '?lang' => 'http://localhost/C++', '?project' => 'http://localhost/DEF', '?visit' => '2021-04-12T15:24:45', '?since' => '2015-05-23', '?year'=> '1979', '?software' => '' ],
	        [ '?person' => 'http://localhost/Jill', '?name' => 'Jill Doe', '?lang' => '', '?project' => '', '?visit' => '2021-04-12T15:28:12', '?since' => '2015-05-18', '?year' => '1990', '?software' => 'http://localhost/Photoshop' ] ],
	      'numRecords' => 14 ],

	    [ 'hasResult' => // 3
	      [ [ '?person' => 'http://localhost/Jack', '?name' => 'Jack Doe', '?lang' => 'http://localhost/C++', '?project' => 'http://localhost/ABC' ],
	        [ '?person' => 'http://localhost/Jack', '?name' => 'Jack Doe', '?lang' => 'http://localhost/C++', '?project' => 'http://localhost/DEF' ],
	        [ '?person' => 'http://localhost/Jack', '?name' => 'Джек Доу', '?lang' => 'http://localhost/JavaScript', '?project' => 'http://localhost/DEF' ],
	        [ '?person' => 'http://localhost/Jack', '?name' => 'Jack Doe', '?lang' => 'http://localhost/JavaScript', '?project' => 'http://localhost/ABC' ],
	        [ '?person' => 'http://localhost/Jack', '?name' => 'Jack Doe', '?lang' => 'http://localhost/PHP', '?project' => 'http://localhost/DEF' ] ],
	     'numRecords' => 13 ],

	    [ 'hasResult' => // 4
	      [ [ '?lang' => 'http://localhost/C++', '?property' => 'http://www.w3.org/2000/01/rdf-schema#label', '?name' => 'C++' ], 
	        [ '?lang' => 'http://localhost/PHP', '?property' => 'http://www.w3.org/2000/01/rdf-schema#label', '?name' => 'PHP' ], 
	        [ '?lang' => 'http://localhost/JavaScript', '?property' => 'http://www.w3.org/2000/01/rdf-schema#label', '?name' => 'JavaScript' ] ],
	      'numRecords' => 3 ],

	    [ 'hasResult' => // 5
	      [ [ '?person' => 'http://localhost/Jack', '?lang' => 'http://localhost/C++', '?class' => 'http://localhost/Programmer', '?property' => 'http://localhost/knows' ] ],
	      'numRecords' => 4 ],

	    [ 'hasResult' => // 6
	      [ [ '?person1' => 'http://localhost/Jack', '?project' => 'http://localhost/ABC', '?lang1' => 'http://localhost/C++', '?type1' => 'http://localhost/Compilable', '?person2' => 'http://localhost/Jane', '?lang2' => 'http://localhost/C++', '?type2' => 'http://localhost/Compilable' ],
	        [ '?person1' => 'http://localhost/Jane', '?lang1' => 'http://localhost/C++', '?type1' => 'http://localhost/Compilable', '?person2' => 'http://localhost/Jack', '?project' => 'http://localhost/ABC', '?lang2' => 'http://localhost/PHP', '?type2' => 'http://localhost/Interpretable' ] ],
	      'numRecords' => 12 ],

	    [ 'hasResult' => // 7
	      [ [ '?person' => 'http://localhost/Jack', '?lang1' => 'http://localhost/C++', '?name1' => 'C++', '?project' => 'http://localhost/ABC', '?lang2' => 'http://localhost/C++', '?name2' => 'C++' ],
	        [ '?person' => 'http://localhost/Jack', '?lang1' => 'http://localhost/PHP', '?name1' => 'PHP', '?project' => 'http://localhost/DEF', '?lang2' => 'http://localhost/PHP', '?name2' => 'PHP' ],
	        [ '?person' => 'http://localhost/Jane', '?lang1' => 'http://localhost/C++', '?name1' => 'C++', '?project' => 'http://localhost/ABC', '?lang2' => 'http://localhost/C++', '?name2' => 'C++' ] ],
	      'numRecords' => 6 ],

	    [ 'hasResult' => // 8
	      [ [ '?project' => 'http://localhost/ABC', '?person' => 'http://localhost/Jack', '?name1' => 'C++', '?lang1' => 'http://localhost/C++', '?lang2' => 'http://localhost/C++' ],
	        [ '?project' => 'http://localhost/DEF', '?person' => 'http://localhost/Jack', '?name1' => 'C++', '?lang1' => 'http://localhost/C++', '?lang2' => 'http://localhost/PHP' ] ],
	      'numRecords' => 16 ],

	    [ 'hasResult' => // 9
	      [ [ 
	      '?programmer' => 'http://localhost/Programmer', '?person' => 'http://localhost/Jack', '?name' => 'Jack Doe', '?year' => '1979', '?lang' => 'http://localhost/PHP', '?subclass' => 'http://localhost/Programmer', '?person2' => 'http://localhost/Jax', '?namelang' => 'PHP', '?works' => '2015-05-23','?works2' => '2019-01-18', '?name2' => '', '?lang2' => '' ] ],
	      'numRecords' => 72 ],

	    [ 'hasResult' => // 10
	      [ [ 
	      '?project' => 'http://localhost/DEF', '?person' => 'http://localhost/Jane', '?person2' => 'http://localhost/Jack', '?otherproj' => 'http://localhost/ABC', '?otherproj_label' => 'ABC' ] ],
	      'numRecords' => 7 ],

	    [ 'hasResult' => // 11
	      [ [ 
	      '?person' => 'http://localhost/Jack', '?predicate' => 'http://localhost/knows', '?value' => 'http://localhost/C++', '?object' => 'http://localhost/PHP' ] ],
	      'numRecords' => 621 ],

	    [ 'hasResult' => // 12
	      [ [ 
	      '?subject' => 'http://localhost/Jack', '?predicate' => 'http://localhost/knows' ] ],
	      'numRecords' => 2 ],

	    [ 'hasResult' => // 13
	      [ [ '?person' => 'http://localhost/Jack', '?lang' => 'http://localhost/JavaScript' ] ],
	      'numRecords' => 1 ],

	    [ 'hasResult' => // 14
	      [ [ '?object' => 'http://localhost/Compilable', '?name' => 'Compilable' ] ],
	      'numRecords' => 1 ],

	    [ 'hasResult' => // 15
	      [ [ '?person' => 'http://localhost/Jack', '?name' => 'Jack Doe' ],
		[ '?person' => 'http://localhost/Jane', '?name' => 'Jane Doe' ] ],
	      'numRecords' => 3 ],

	    [ 'hasResult' => // 16
	      [ [ '?person' => 'http://localhost/Jack' ],
		[ '?person' => 'http://localhost/Jane' ] ],
	      'numRecords' => 2 ],

	    [ 'hasResult' => // 17
	      [ [ '?person' => 'http://localhost/Jack', '?predicate' => 'http://localhost/knows' ],
		[ '?person' => 'http://localhost/Jane', '?predicate' => 'http://localhost/knows' ] ],
	      'numRecords' => 2 ],

	    [ 'hasResult' => // 18
	      [ [ '?person' => 'http://localhost/Jack', '?name' => 'Jack Doe' ] ],
	      'numRecords' => 1 ],

	    [ 'hasResult' => // 19
	      [ [ '?person' => 'http://localhost/Jane', '?name' => 'Jane Doe' ] ],
	      'numRecords' => 2 ],

	    [ 'hasResult' => // 20
	      [ [ '?project' => 'http://localhost/ABC', '?language' => 'http://localhost/C++', '?person' => 'http://localhost/Jane' ] ],
	      'numRecords' => 2 ],

	    [ 'hasResult' => // 21
	      [ [ '?person' => 'http://localhost/Jane', '?name' => 'Jane Doe' ] ],
	      'numRecords' => 1 ],

	    [ 'hasResult' => // 22
	      [ [ '?person' => 'http://localhost/Jill', '?name' => 'Jill Doe' ] ],
	      'numRecords' => 1 ],

	    [ 'hasResult' => // 23
	      [ [ '?person' => 'http://localhost/Jill' ] ],
	      'numRecords' => 1 ],

	    [ 'hasResult' => // 24
	      [ [ '?person' => 'http://localhost/Jill', '?predicate' => 'http://localhost/usesSoftware', '?software' => 'http://localhost/Photoshop', '?project' => '' ],
		[ '?person' => 'http://localhost/Jack', '?predicate' => 'http://localhost/participatesIn', '?project' => 'http://localhost/ABC', '?software' => '' ] ],
	      'numRecords' => 15 ],

	    [ 'hasResult' => // 25
	      [ [ '?a' => 'http://localhost/Jack', '?b' => 'http://www.w3.org/2000/01/rdf-schema#label', '?c' => 'Jack Doe' ] ],
	      'numRecords' => 6 ],

	];

echo("\n3. Running complex tests\n");
foreach( $tests as $ind => $test ) {
/*
if( $ind != 12 ) continue;
echo("test:\n");
print_r($test);
echo("result:\n");
print_r($answers[ $ind ][ 'hasResult' ]);
*/
	echo($ind . ". ");
	$st = microtime(true);
	$res = request('GET', 'chain', $test );
	$et = microtime(true);
	if($res->Status != 'Ok') {
		echo("ERROR!");
		continue;
	}

	$error = false;
	foreach( $answers[ $ind ][ 'hasResult' ] as $result ) {
		$found = false;
		foreach( $res->Result as $rr ) {
			$rs = [];
			foreach( $rr as $n => $r ) {
				$rs[ $res->Vars[$n] ] = $r[0];
			}
			if( sizeof( $rs ) != sizeof( $result ) ) {
				echo( "Result size mismatch: " . sizeof( $result ) . " instead of " . sizeof( $rs ) . "\n");
				$error = true;
				break 2;
			}
			if( !sizeof( array_diff_assoc( $result, $rs ) ) && !sizeof( array_diff_assoc( $rs, $result ) ) )
				$found = true;
/*			else {
			echo("arrays:\n");
			print_r($result);
			print_r($rs);
			echo("diff:\n");
			print_r(array_diff_assoc( $result, $rs ));
			print_r(array_diff_assoc( $rs, $result ));
			} */
		}
		if( !$found ) {
			echo( "Result not found:\n" . print_r( $result, true ) . "\n" );
			$error = true;
		}
	}

	if( sizeof($res->Result) != $answers[ $ind ][ 'numRecords' ] ) {
		echo( "Number of records mismatch: " . sizeof($res->Result) . " instead of " . $answers[ $ind ][ 'numRecords' ] . "\n" );
		$error = true;
	}

	if( $error ) {
		echo(" ERROR");
		// Print table
/*		foreach($res->Vars as $rr) {
		    echo($rr."\t");
		}
		echo("\n");
		foreach($res->Result as $rr) {
		    foreach($rr as $r) {
			echo($r[0]."\t");
		    }
		    echo("\n");
		} */
	}
	else
		echo(" OK");
	echo(" ".round($et-$st,3)."\n");
}

?>
