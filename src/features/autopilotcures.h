#pragma once

#include <stddef.h>

struct auto_cure {
	const char *ailment;
	const char *choices[12];
};

static const struct auto_cure AUTO_CURES[] = {
	{"Accidental Gunshot", {"Clean and dress the wound", "Treat with an antiseptic", "Rest here awhile", "Treat with an astringent", "Increase rations", "Continue as usual", "Rub salt in the wound", NULL}},
	{"Animal Bite", {"Clean and dress the wound", "Treat with an antiseptic", "Rest here awhile", "Treat with an astringent", "Increase rations", "Continue as usual", "Rub salt in the wound", NULL}},
	{"Animal Mauling", {"Clean and dress the wound", "Treat with an antiseptic", "Rest here awhile", "Treat with an astringent", "Increase rations", "Continue as usual", "Rub salt in the wound", NULL}},
	{"Snakebite", {"Use tourniquet/suction method", "Rest here awhile", "Use antiseptic on the bite", "Continue as usual", "Slow down", "Increase rations", "Give patient lots of exercise", NULL}},
	{"Burns", {"Clean and bandage burns", "Apply antiseptic to burns", "Rest here awhile", "Increase rations", "Apply aloe to burns", "Continue as usual", "Apply grease or lard to burns", NULL}},
	{"Concussion", {"Rest here awhile", "Use camphor for stimulation", "Slow down", "Increase rations", "Administer laudanum", "Continue as usual", "Keep the patient very active", NULL}},
	{"Cuts and Abrasions", {"Clean and dress the wounds", "Treat with an antiseptic", "Rest here awhile", "Treat with an astringent", "Increase rations", "Continue as usual", "Rub salt in the wounds", NULL}},
	{"Internal Injuries", {"Administer laudanum", "Rest here awhile", "Make patient comfortable", "Slow down", "Increase rations", "Continue as usual", "Administer magnesia", NULL}},
	{"Drowning", {"Rest here awhile", "Continue", NULL}},
	{"Broken Arm", {"Set the bone and apply splint", "Rest here awhile", "Slow down", "Increase rations", "Continue as usual", "Apply camphor to the break", "Exercise the broken bone", NULL}},
	{"Broken Leg", {"Set the bone and apply splint", "Rest here awhile", "Slow down", "Increase rations", "Continue as usual", "Apply camphor to the break", "Exercise the broken bone", NULL}},
	{"Broken Foot", {"Set the bone and apply splint", "Rest here awhile", "Slow down", "Increase rations", "Continue as usual", "Apply camphor to the break", "Exercise the broken bone", NULL}},
	{"Broken Hip", {"Set the bone and apply splint", "Rest here awhile", "Slow down", "Increase rations", "Continue as usual", "Apply camphor to the break", "Exercise the broken bone", NULL}},
	{"Broken Wrist", {"Set the bone and apply splint", "Rest here awhile", "Slow down", "Increase rations", "Continue as usual", "Apply camphor to the break", "Exercise the broken bone", NULL}},
	{"Sprained Wrist", {"Immobilize affected joint", "Soak in warm epsom saltwater", "Rest here awhile", "Slow down", "Apply alchohol to the sprain", "Continue as usual", "Exercise the affected joint", NULL}},
	{"Sprained Elbow", {"Immobilize affected joint", "Soak in warm epsom saltwater", "Rest here awhile", "Slow down", "Apply alchohol to the sprain", "Continue as usual", "Exercise the affected joint", NULL}},
	{"Sprained Knee", {"Immobilize affected joint", "Soak in warm epsom saltwater", "Rest here awhile", "Slow down", "Apply alchohol to the sprain", "Continue as usual", "Exercise the affected joint", NULL}},
	{"Sprained Ankle", {"Immobilize affected joint", "Soak in warm epsom saltwater", "Rest here awhile", "Slow down", "Apply alchohol to the sprain", "Continue as usual", "Exercise the affected joint", NULL}},
	{"Sprained Shoulder", {"Immobilize affected joint", "Soak in warm epsom saltwater", "Rest here awhile", "Slow down", "Apply alchohol to the sprain", "Continue as usual", "Exercise the affected joint", NULL}},
	{"Bad Cold", {"Increase rations", "Continue as usual", "Rest here awhile", "Slow down", "Increase fluid intake", "Administer vinegar", "Administer olive oil", NULL}},
	{"The Grippe", {"Rest here awhile", "Administer lecithin", "Increase fluid intake", "Administer vinegar", "Slow down", "Continue as usual", "Administer quinine", NULL}},
	{"Pneumonia", {"Rest here awhile", "Keep the patient warn and dry", "Slow down", "Increase rations", "Administer laudanum", "Continue as usual", "Keep the patient cool and moist", NULL}},
	{"Consumption", {"Rest here awhile", "Keep patient warm and dry", "Slow down", "Increase rations", "Administer laudanum", "Continue as usual", "Keep patient cool and moist", NULL}},
	{"Diphtheria", {"Rest here awhile", "Increase fluid intake", "Slow down", "Increase rations", "Administer vinegar", "Continue as usual", "Administer Dover's Powder", NULL}},
	{"Cholera", {"Rest here awhile", "Administer peppermint", "Administer epsom salts", "Increase rations", "Administer laudanum", "Continue as usual", NULL}},
	{"Dysentery", {"Rest here awhile", "Increase fluid and salt intake", "Administer peppermint", "Increase rations", "Administer laudanum", "Continue as usual", "Administer magnesia", NULL}},
	{"Typhoid Fever", {"Try to control the fever", "Rest here awhile", "Slow down", "Increase rations", "Administer laudanum", "Apply aloe to the skin rash", "Continue as usual", NULL}},
	{"Typhus", {"Rest here awhile", "Slow down", "Increase rations", "Administer laudanum", "Apply aloe to the skin rash", "Continue as usual", "Keep patient cool and moist", NULL}},
	{"Mountain Fever", {"Rest here awhile", "Try to control the fever", "Slow down", "Increase rations", "Treat the skin rash with aloe", "Continue as usual", "Administer magnesia", NULL}},
	{"Malaria", {"Administer quinine", "Increase rations", "Administer sassafras", "Rest here awhile", "Slow down", "Administer magnesia", "Continue as usual", NULL}},
	{"Measles", {"Rest here awhile", "Increase fluid intake", "Treat the skin rash with aloe", "Slow down", "Increase rations", "Continue as usual", "Administer epsom salts", NULL}},
	{"Scarlet Fever", {"Keep patient cool and moist", "Rest here awhile", "Increase fluid intake", "Administer laudanum", "Slow down", "Increase rations", "Continue as usual", NULL}},
	{"Smallpox", {"Rest here awhile", "Slow down", "Increase rations", "Apply aloe to the skin lesions", "Use antiseptic on the skin lesions", "Continue as usual", "Administer epsom salts", NULL}},
	{"Lockjaw", {"Rest here awhile", "Make patient comfortable", "Administer laudanum", "Feed patient fluids", "Increase rations", "Slow down", "Continue as usual", "Administer epsom salts", NULL}},
	{"Rabies", {"Make patient comfortable", "Administer laudanum", "Rest here awhile", "Increase rations", "Continue as usual", "Slow down", "Administer magnesia", "Administer epsom salts", NULL}},
	{"Freezing", {"Stop and build a fire", "Feed patient warm food", "Continue", "Rest here awhile", "Slow down", NULL}},
	{"Frostbite", {"Gradually warm affected area", "Rest here awhile", "Slow down", "Increase rations", "Quickly warm affected area", "Continue as usual", "Rub affected area with snow", NULL}},
	{"Starvation", {"Kill a draft animal for food", "Go hunting", "Continue", "Rest here awhile", NULL}},
	{"Thirst", {"Increase rations", "Go faster", "Continue as usual", "Go slower", "Rest here awhile", NULL}},
	{"Scurvy", {"Go look for edible plants", "Administer Dover's Powder", "Increase rations", "Continue as usual", "Rest here awhile", "Slow down", "Administer magnesia", NULL}},
	{"Beriberi", {"Go hunting", "Increase rations", "Continue as usual", "Rest here awhile", "Slow down", "Administer Duffy\x92s Elixer", "Administer Dover's Powder", NULL}},
	{"Food Poisoning", {"Rest here awhile", "Increase fluid and salt intake", "Administer laudanum", "Administer peppermint", "Continue as usual", "Increase rations", "Administer epsom salts", NULL}},
	{"Alkali Sickness", {"Administer a dose of vinegar", "Administer flour and water", "Administer grease or lard", "Rest here awhile", "Increase rations", "Continue as usual", "Administer baking soda", NULL}},
	{"Water Poisoning", {"Administer peppermint", "Rest here awhile", "Slow down", "Increase rations", "Continue as usual", NULL}},
	{"Gangrene", {"Amputate the affected limb", "Rest here awhile", "Increase rations", "Continue as usual", "Slow down", "Apply sulfur to the wound", "Administer Duffy\x92s Elixer", NULL}},
	{"Infection", {"Clean and dress wound", "Apply sulfur to wound", "Apply antiseptic to wound", "Rest here awhile", "Increase rations", "Continue as usual", "Rub salt in the wound", NULL}},
	{"Wound", {"Clean and dress the wound", "Apply sulfur to the wound", "Use an antiseptic on the wound", "Administer laudanum", "Rest here awhile", "Continue as usual", "Rub salt in the wound", NULL}},
	{NULL, {NULL}}
};
