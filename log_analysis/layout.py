#!/usr/bin/python3 
import pandas as pd
import networkx as nx
import json
import sys


def get_geo_info(country_dict,as_dict,ip):
    result = {}
    result['id']= ip
    result['country'] = country_dict[ip]
    result['description'] = str(as_dict[ip]) +':'+ ip
    result 
    return result

def get_bfs(country_list,as_list,graph,ip,depth):
    ip_set= set() 
    bfslist=list(nx.bfs_edges(graph, ip ,depth_limit=depth))    
    link_list =[]
    for edge in bfslist:
        src = edge[0]
        dst = edge[1]
        link={}
        link['source'] = src
        link['target'] = dst
        link_list.append(link)
        ip_set.add(src)
        ip_set.add(dst)
    node_list=[]
    for addr in iter(ip_set):
        node = get_geo_info(country_list,as_list,addr)
        node_list.append(node)
    result = {}
    result['nodes'] = node_list
    result['links'] = link_list
    return json.dumps(result)


df = pd.read_csv("aaa.csv",delimiter="|",names=['src','src_country','src_as','dst','dst_country','dst_as'])
country_dict = dict(zip(df['src'],df['src_country']))
dst_country_dict = dict(zip(df['dst'],df['dst_country']))
country_dict.update(dst_country_dict)

as_dict = dict(zip(df['src'],df['src_as']))
dst_as_dict = dict(zip(df['dst'],df['dst_as']))
as_dict.update(dst_as_dict)

G = nx.Graph()
for index, row in df.iterrows():
    G.add_edge(row['src'],row['dst'])

result = get_bfs(country_dict,as_dict,G,sys.argv[1],int(sys.argv[2]))
print(result)
