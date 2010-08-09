//-----------------------------------------------
// Copyright 2010 Wellcome Trust Sanger Institute
// Written by Jared Simpson (js18@sanger.ac.uk)
// Released under the GPL
//-----------------------------------------------
//
// ScaffoldGraph - A graph representing long-distance
// relationships between contigs.
//
#include "ScaffoldGraph.h"
#include "SeqReader.h"

ScaffoldGraph::ScaffoldGraph()
{
    m_vertices.set_deleted_key("");
}

//
ScaffoldGraph::~ScaffoldGraph()
{
    for(ScaffoldVertexMap::iterator iter = m_vertices.begin();
         iter != m_vertices.end(); ++iter)
    {
        iter->second->deleteEdges();
        delete iter->second;
        iter->second = NULL;
    }
}

//
void ScaffoldGraph::addVertex(ScaffoldVertex* pVertex)
{
    m_vertices.insert(std::make_pair(pVertex->getID(), pVertex));
}

//
void ScaffoldGraph::addEdge(ScaffoldVertex* pVertex, ScaffoldEdge* pEdge)
{
    assert(pVertex != NULL);
    pVertex->addEdge(pEdge);
}

//
ScaffoldVertex* ScaffoldGraph::getVertex(VertexID id) const
{
    ScaffoldVertexMap::const_iterator iter = m_vertices.find(id);
    if(iter == m_vertices.end())
        return NULL;
    return iter->second;
}

//
void ScaffoldGraph::deleteVertices(ScaffoldVertexClassification classification)
{
    ScaffoldVertexMap::iterator iter = m_vertices.begin(); 
    while(iter != m_vertices.end())
    {
        if(iter->second->getClassification() == classification)
        {
            iter->second->deleteEdgesAndTwins();
            delete iter->second;
            iter->second = NULL;
            m_vertices.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }
}

//
void ScaffoldGraph::setVertexColors(GraphColor c)
{
    ScaffoldVertexMap::iterator iter = m_vertices.begin(); 
    while(iter != m_vertices.end())
    {
        iter->second->setColor(c);
        ++iter;
    }
}

// 
void ScaffoldGraph::loadVertices(const std::string& filename, int minLength)
{
    SeqReader reader(filename);
    SeqRecord sr;
    while(reader.get(sr))
    {
        int contigLength = sr.seq.length();
        if(contigLength >= minLength)
        {
            ScaffoldVertex* pVertex = new ScaffoldVertex(sr.id, sr.seq.length());
            addVertex(pVertex);
        }
    }    
}

// 
void ScaffoldGraph::loadDistanceEstimateEdges(const std::string& filename)
{
    std::istream* pReader = createReader(filename);
    std::string line;

    while(getline(*pReader, line))
    {
        assert(line.substr(0,4) != "Mate");
        StringVector fields = split(line, ' ');
        assert(fields.size() >= 1);

        std::string rootID = fields[0];
        EdgeDir currDir = ED_ANTISENSE;

        for(size_t i = 1; i < fields.size(); ++i)
        {
            std::string record = fields[i];
            if(record == ";")
            {
                currDir = !currDir;
                continue;
            }

            std::string id;
            EdgeComp comp;
            int distance;
            int numPairs;
            double stdDev;
            parseDERecord(record, id, comp, distance, numPairs, stdDev);

            std::cout << "Link from " << rootID << " to " << id << " comp " << comp << 
                         " distance " << distance << " dir " << currDir << " np " << 
                         numPairs << " sd " << stdDev << "\n";

            // Get the vertices that are linked
            ScaffoldVertex* pVertex1 = getVertex(rootID);
            ScaffoldVertex* pVertex2 = getVertex(id);

            if(pVertex1 != NULL && pVertex2 != NULL)
            {
                if(pVertex1 == pVertex2)
                {
                    std::cout << "Self-edges not allowed\n";
                    continue;
                }

                // Check if there already exists a DistanceEstimate edge between these vertices
                ScaffoldEdge* pEdge = pVertex1->findEdgeTo(id, SET_DISTANCEEST);
                if(pEdge != NULL)
                {
                    // An edge to this vertex already exists
                    std::cerr << "Update duplicate edges\n";
                }
                else
                {
                    ScaffoldEdge* pEdge1 = new ScaffoldEdge(pVertex2, currDir, comp, distance, stdDev, numPairs, SET_DISTANCEEST);
                    ScaffoldEdge* pEdge2 = new ScaffoldEdge(pVertex1, !correctDir(currDir, comp), comp, distance, stdDev, numPairs, SET_DISTANCEEST);
                    pEdge1->setTwin(pEdge2);
                    pEdge2->setTwin(pEdge1);

                    addEdge(pVertex1, pEdge1);
                    addEdge(pVertex2, pEdge2);
                }
            }
        }
    }

    delete pReader;
}

void ScaffoldGraph::loadAStatistic(const std::string& filename)
{
    std::istream* pReader = createReader(filename);
    std::string line;

    while(getline(*pReader, line))
    {
        StringVector fields = split(line, '\t');
        assert(fields.size() == 6);

        VertexID id = fields[0];
        std::stringstream parser(fields[5]);
        double as;
        parser >> as;

        ScaffoldVertex* pVertex = getVertex(id);
        if(pVertex != NULL)
            pVertex->setAStatistic(as);
    }
}


//
void ScaffoldGraph::parseDERecord(const std::string& record, std::string& id, 
                                  EdgeComp& comp, int& distance, int& numPairs, double& stdDev)
{
    StringVector fields = split(record, ',');
    if(fields.size() != 4)
    {
        std::cerr << "Distance Estimate record is not formatted correctly: " << record << "\n";
        exit(1);
    }

    // Parse the ID and its orientation
    id = fields[0].substr(0, fields[0].size() - 1);
    comp = (fields[0][fields[0].size() - 1] == '+' ? EC_SAME : EC_REVERSE);

    std::stringstream d_parser(fields[1]);
    d_parser >> distance;
    
    std::stringstream np_parser(fields[2]);
    np_parser >> numPairs;

    std::stringstream sd_parser(fields[3]);
    sd_parser >> stdDev;
}

//
void ScaffoldGraph::writeDot(const std::string& outFile) const
{
    std::ostream* pWriter = createWriter(outFile);
    
    std::string graphType = "digraph";

    *pWriter << graphType << " G\n{\n";
    ScaffoldVertexMap::const_iterator iter = m_vertices.begin(); 
    for(; iter != m_vertices.end(); ++iter)
    {
        iter->second->writeDot(pWriter);
    }
    *pWriter << "}\n";
    delete pWriter;
}
