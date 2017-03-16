#include "SerialPlanner.h"
#include <queue>
#include <set>



SerialPlanner::SerialPlanner()
{
	srand(time(NULL));
}

void SerialPlanner::reset_queued_flags(map<int, CourseNode*>* _crs_details)
{
	for (auto& crs : (*_crs_details))
	{
		crs.second->isQueued = false;
	}
}




DegreePlan SerialPlanner::chain_end_courses(map<AugNode*, CourseMatrix>& paths_map, QuarterNode start_qtr, map<int, CourseNode*>* _crs_details)
{
	DegreePlan qtr_map;
	unsigned short start_yr = start_qtr.year;
	for (int i = 0; i < 4; i++)
	{
		qtr_map.insert(pair<QuarterNode, vector<CourseNode*>>(start_qtr, vector<CourseNode*>()));
		start_qtr = start_qtr.next_qtr();
	}
	for (auto target = paths_map.begin(); target != paths_map.end(); target++)
	{
		CourseNode* course = (*_crs_details)[target->first->course_ids[0]];
		QuarterNode qtr;
		qtr.quarter = course->quarters[0];
		qtr.year = start_yr;
		if (qtr_map.find(qtr) == qtr_map.end())
			qtr.year++;
		qtr_map[qtr].push_back(course);
		course->isQueued = true;
	}
	return qtr_map;
}


void SerialPlanner::merge_paths_into_qtr_chain(DegreePlan& qtr_chain, vector<AugNode*> path, ushort year, map<int, CourseNode*>* _crs_details)
{

	for (auto crs = path.begin(); crs != path.end(); crs++)
	{

		for (auto crs_id : (*crs)->course_ids)
		{
			CourseNode* course = (*_crs_details)[crs_id];
			if (!course->isQueued)
			{
				QuarterNode qtr;
				qtr.quarter = (*_crs_details)[crs_id]->quarters[0];
				qtr.year = year;
				place_crs_in_chain(qtr_chain, course, qtr, _crs_details);
			}


		}


	}
}

void SerialPlanner::place_crs_in_chain(DegreePlan& qtr_chain, CourseNode* crs, QuarterNode qtr, map<int, CourseNode*>* _crs_details)
{
	vector<CourseNode*>* test_qtr_crses = nullptr;
	if (qtr < qtr_chain.begin()->first)
	{
		qtr.year++; //meant for the next year
	}
	if (qtr_chain.find(qtr) == qtr_chain.end())
	{
		QuarterNode to_add = qtr_chain.rbegin()->first;
		while (to_add != qtr)
		{
			to_add = to_add.next_qtr();
			qtr_chain.insert(pair<QuarterNode, vector<CourseNode*>>(to_add, vector<CourseNode*>()));
		}
	}

	test_qtr_crses = &qtr_chain[qtr];

	bool add_crs = true;
	set<pair<CourseNode*, QuarterNode>> crs_for_deference; //using set to avoid duplicate entries in the deferrence list
	auto test_qtr_itr = qtr_chain.find(qtr);

	//check all quarters before current quarter under consideration
	for (auto qtr_itr = qtr_chain.begin(); qtr_itr != test_qtr_itr; qtr_itr++)
	{
		for (auto c = qtr_itr->second.begin(); c != qtr_itr->second.end(); c++)
		{
			CourseNode* c_val = *c;
			auto req_ptr = find((*_crs_details)[crs->course_code]->post_reqs.begin(), (*_crs_details)[crs->course_code]->post_reqs.end(), c_val->course_code);
			if (req_ptr != (*_crs_details)[crs->course_code]->post_reqs.end())
			{//crs is a pre-requisite to c 
				crs_for_deference.insert(pair<CourseNode*, QuarterNode>(*c, qtr_itr->first)); //add c to a list of courses to be deferred
			}

		}
	}
	if (!test_qtr_crses->empty())
	{//quarter under consideration have courses already assigned to it

		for (auto c = test_qtr_crses->begin(); c != test_qtr_crses->end(); c++)
		{
			auto req_ptr = find((*_crs_details)[(*c)->course_code]->post_reqs.begin(), (*_crs_details)[(*c)->course_code]->post_reqs.end(), crs->course_code);

			CourseNode* c_val = *c;
			auto rev_req_ptr = find((*_crs_details)[crs->course_code]->post_reqs.begin(), (*_crs_details)[crs->course_code]->post_reqs.end(), c_val->course_code);
			if (req_ptr != (*_crs_details)[(*c)->course_code]->post_reqs.end())
			{ //c is a pre-requisite  to crs
				add_crs = false; //do not add crs to this quarter
				break;
			}
			else if (rev_req_ptr != (*_crs_details)[crs->course_code]->post_reqs.end())
			{//crs is a pre-requisite to c 

				crs_for_deference.insert(pair<CourseNode*, QuarterNode>(*c, qtr)); //add c to a list of courses to be deferred
			}
		}

	}
	if (add_crs)
	{
		//check quarters after the one under consideration
		for (auto next_qtrs = ++test_qtr_itr; next_qtrs != qtr_chain.end(); next_qtrs++)
		{
			for (auto next_qtr_crs = next_qtrs->second.begin(); next_qtr_crs != next_qtrs->second.end(); next_qtr_crs++)
			{
				CourseNode* c_val = *next_qtr_crs;
				auto req_ptr = find((*_crs_details)[c_val->course_code]->post_reqs.begin(), (*_crs_details)[c_val->course_code]->post_reqs.end(), crs->course_code);
				if (req_ptr != (*_crs_details)[c_val->course_code]->post_reqs.end())
				{
					//c_val is a pre-requisite to crs
					add_crs = false;
					break;
				}
			}
			if (!add_crs)
			{
				break;
			}
		}
	}

	if (add_crs)
	{
		//add crs to current quarter
		test_qtr_crses->push_back(crs);
		crs->isQueued = true;
	}
	else
	{
		//if crs is not going to be added, include it in the deference list
		crs_for_deference.insert(pair<CourseNode*, QuarterNode>(crs, qtr));
	}
	for (auto deferee = crs_for_deference.begin(); deferee != crs_for_deference.end(); deferee++)
	{
		//clean up deferred courses from current quarter chain
		vector<CourseNode*>& qtr_crses = qtr_chain[deferee->second];
		auto remove = find(qtr_crses.begin(), qtr_crses.end(), deferee->first);
		if (remove != qtr_crses.end())
		{ //remove if present
			qtr_crses.erase(remove);
		}
	}
	for (auto deferee = crs_for_deference.begin(); deferee != crs_for_deference.end(); deferee++)
	{
		//cycle to next placeable quarter
		place_crs_in_chain(qtr_chain, deferee->first, get_crs_next_feasible_qtr(deferee->first, deferee->second), _crs_details);
	}

}

void SerialPlanner::resolve_clashes(map<int, DegreePlan>& plans, map<int, CourseNode*>* _crs_details)
{
	final_output = plans;
	for (auto plan = plans.begin(); plan != plans.end(); plan++)
	{
		reorder_for_clash(plans, plan->second, plan->first, 0, true, _crs_details);
	}
}

void SerialPlanner::reorder_for_clash(map<int, DegreePlan>& plans, DegreePlan& single_plan, int plan_id, int start_index, bool is_included, map<int, CourseNode*>* _crs_details)
{

	bool remove_plan = false;
	DegreePlan::iterator qtr_start = single_plan.begin();
	int offset = 0;

	while (offset++ < start_index)
	{
		qtr_start++;
	}

	for (auto qtr = qtr_start; qtr != single_plan.end(); qtr++)
	{
		int size = static_cast<int>(qtr->second.size());
		for (int i = 0; i < size - 1; i++)
		{
			bool found_overlap = false;
			for (int j = i + 1; j < size; j++)
			{
				//check if a course overlaps with another
				CourseNode* crs1 = qtr->second[i];
				CourseNode* crs2 = qtr->second[j];
				for (schedule& sch1 : crs1->schedules)
				{
					for (schedule& sch2 : crs2->schedules)
					{
						if (sch1.day == sch2.day)
						{
							if (sch1.time.second > sch2.time.first &&
								sch1.time.first <= sch2.time.second)
							{
								found_overlap = true;
								remove_plan = is_included;
								//option 1: defer crs 1 with new plan emerging		
								DegreePlan new_plan1 = single_plan;
								new_plan1[qtr->first].erase(new_plan1[qtr->first].begin() + i);
								QuarterNode newQtr = get_crs_next_feasible_qtr(crs1, qtr->first);
								place_crs_in_chain(new_plan1, crs1, newQtr, _crs_details);
								reorder_for_clash(plans, new_plan1, glb_plan_id++, start_index, false, _crs_details);
								//option 2: defer crs 2 with another new plan emerging
								DegreePlan new_plan2 = single_plan;
								newQtr = get_crs_next_feasible_qtr(crs2, qtr->first);
								new_plan2[qtr->first].erase(new_plan2[qtr->first].begin() + j);
								place_crs_in_chain(new_plan1, crs2, newQtr, _crs_details);
								reorder_for_clash(plans, new_plan2, glb_plan_id++, start_index, false, _crs_details);
								break;
							}
						}
					}

					if (found_overlap)
					{
						break;
					}
				}

			}
			if (found_overlap)
			{
				break;
			}

		}
		start_index++;
	}
	if (remove_plan)
	{
		final_output.erase(final_output.find(plan_id));
	}
	else if (!is_included)
	{
		final_output.insert(pair<int, DegreePlan>(glb_plan_id, single_plan));
	}
}

map<int, DegreePlan> SerialPlanner::phase2(map<AugNode*, CourseMatrix>& paths_map_input, QuarterNode start_qtr, map<int, CourseNode*>* _crs_details)
{
	AugNode* target_crs = paths_map_input.begin()->first; //get target with the maximum number of paths to generate the most plans possible
	for (auto trgt = ++paths_map_input.begin(); trgt != paths_map_input.end(); trgt++)
	{
		if (trgt->second.size() > paths_map_input[target_crs].size())
		{
			target_crs = trgt->first;
		}
	}

	map<int, DegreePlan> output;
	for (auto path = paths_map_input[target_crs].begin(); path != paths_map_input[target_crs].end(); path++)
	{
		reset_queued_flags(_crs_details);  //reset isQueued flags

		DegreePlan qtr_chain = chain_end_courses(paths_map_input, start_qtr, _crs_details); //fresh output copy

		merge_paths_into_qtr_chain(qtr_chain, *path, start_qtr.year, _crs_details);
		for (auto other_crses = ++paths_map_input.begin(); other_crses != paths_map_input.end(); other_crses++)
		{
			merge_paths_into_qtr_chain(qtr_chain, other_crses->second[rand() % other_crses->second.size()], start_qtr.year, _crs_details);
		}
		//emit new plan
		output.insert(pair<int, DegreePlan>(glb_plan_id++, qtr_chain));
	}
	//resolve all class periods overlap
	resolve_clashes(output, _crs_details);
	return final_output;
}

